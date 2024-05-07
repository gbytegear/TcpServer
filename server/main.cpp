#include "tcp/include/TcpServer.h"

#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <shared_mutex>

using namespace stcp;

// ---------------------------------------------------------------------------------------------------------------------
// Entries

struct UserEntry {
  std::string nickname{};
  std::string password_hash{};

  struct Comparator {
    using is_transparent = std::true_type;

    bool operator()(const UserEntry& lhs, const UserEntry& rhs) const {
      return std::less<std::string>{}(lhs.nickname, rhs.nickname);
    }

    bool operator()(const UserEntry& lhs, const std::string& rhs) const {
      return std::less<std::string>{}(lhs.nickname, rhs);
    }

    bool operator()(const std::string& lhs, const UserEntry& rhs) const {
      return std::less<std::string>{}(lhs, rhs.nickname);
    }

  };
};

struct Message {
  std::time_t send_time;
  std::string content;
};

struct DialogEntry {
  UserEntry* first_user;
  UserEntry* second_user;

};

struct SessionEntry {
  enum Status : unsigned char { unauthorized = 0x00, authorized = 0x01, invalid = 0xFF } status;
  TcpClientBase* socket;
  UserEntry* user_entry = nullptr;
};


// ---------------------------------------------------------------------------------------------------------------------
// Tables

class UserTable {
  std::shared_mutex mutex;
  std::set<UserEntry, UserEntry::Comparator> table;
public:
  UserTable() = default;

  UserEntry* registerUser(std::string nickname, std::string password_hash) {
    std::lock_guard lock(mutex);
    auto result = table.emplace(UserEntry{std::move(nickname), std::move(password_hash)});
    return result.second ? const_cast<UserEntry*>(&*result.first) : nullptr;
  }

  UserEntry* authorize(std::string nickname, std::string password_hash) {
    std::lock_guard lock(mutex);
    if(auto it = table.find(nickname); it != table.cend()) {
      return it->password_hash == password_hash ? const_cast<UserEntry*>(&*it) : nullptr;
    } return nullptr;
  }

  UserEntry* find(std::string nickname) {
    std::shared_lock lock(mutex);
    auto it = table.find(nickname);
    return it != table.cend() ? const_cast<UserEntry*>(&*it) : nullptr;
  }

} user_table;


class SessionTable {

  struct AddressComparator {
    using is_transparent = std::true_type;

    bool operator ()(const SessionEntry& lhs, const SessionEntry& rhs) const {
      return (uint64_t(lhs.socket->getHost()) | uint64_t(lhs.socket->getPort()) << 32) < (uint64_t(rhs.socket->getHost()) | uint64_t(rhs.socket->getPort()) << 32);
    }

    bool operator ()(const SessionEntry& lhs, const stcp::TcpServer::ClientKey& rhs) const {
      return (uint64_t(lhs.socket->getHost()) | uint64_t(lhs.socket->getPort()) << 32) < (uint64_t(rhs.host) | uint64_t(rhs.port) << 32);
    }

    bool operator ()(const stcp::TcpServer::ClientKey& lhs, const SessionEntry& rhs) const {
      return (uint64_t(lhs.host) | uint64_t(lhs.port) << 32) < (uint64_t(rhs.socket->getHost()) | uint64_t(rhs.socket->getPort()) << 32);
    }
  };

  using SessionIterator = std::set<SessionEntry, AddressComparator>::iterator;

  struct UserComparator {
    using is_transparent = std::true_type;

    inline bool operator ()(const SessionIterator& lhs, const SessionIterator& rhs) const {
      return lhs->user_entry < rhs->user_entry;
    }

    inline bool operator ()(const SessionIterator& lhs, UserEntry* rhs) const {
      return lhs->user_entry < rhs;
    }

    inline bool operator ()(UserEntry* lhs, const SessionIterator& rhs) const {
      return lhs < rhs->user_entry;
    }
  };

  mutable std::shared_mutex unique_address_idx_mutex;
  std::set<SessionEntry, AddressComparator> unique_address_idx;
  mutable std::shared_mutex user_idx_mutex;
  std::multiset<SessionIterator, UserComparator> user_idx;

public:
  SessionTable() = default;

  bool addSession(TcpClientBase* socket) {
    std::lock_guard lock(unique_address_idx_mutex);
    return unique_address_idx.emplace(SessionEntry{SessionEntry::Status::unauthorized, socket}).second;
  }

  bool removeSession(TcpClientBase* socket) {
    std::lock_guard lock(unique_address_idx_mutex);
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    if(session_it == unique_address_idx.end()) return false;
    unique_address_idx.erase(session_it);
    return true;
  }

  SessionEntry::Status getStatus(TcpClientBase* socket) const {
    unique_address_idx_mutex.lock_shared();
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    unique_address_idx_mutex.unlock_shared();
    if(session_it == unique_address_idx.end()) return SessionEntry::Status::invalid;
    return session_it->status;
  }

  std::string getNickname(TcpClientBase* socket) const {
    unique_address_idx_mutex.lock_shared();
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    unique_address_idx_mutex.unlock_shared();
    if(session_it == unique_address_idx.end()) return "";
    if(!session_it->user_entry) return "";
    return session_it->user_entry->nickname;
  }

  bool registerUser(TcpClientBase* socket, std::string nickname, std::string password_hash) {
    // Find session
    unique_address_idx_mutex.lock_shared();
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    unique_address_idx_mutex.unlock_shared();
    if(session_it == unique_address_idx.end()) return false;

    // Register new user
    UserEntry* new_user_entry = user_table.registerUser(std::move(nickname), std::move(password_hash));
    if(!new_user_entry) return false;

    std::lock_guard lock(user_idx_mutex);

    // Remove old user from session index
    if(session_it->user_entry) {
      for(auto range = user_idx.equal_range(session_it->user_entry); range.first != range.second; ++range.first) {
        if((*range.first) == session_it) {
          user_idx.erase(range.first);
          break;
        }
      }
    }

    // Set new user to session and add session to index
    const_cast<UserEntry*&>(session_it->user_entry) = new_user_entry;
    const_cast<SessionEntry::Status&>(session_it->status) = SessionEntry::Status::authorized;
    user_idx.emplace(session_it);
    return true;
  }

  bool authorize(TcpClientBase* socket, std::string nickname, std::string password_hash) {
    // Find session
    unique_address_idx_mutex.lock_shared();
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    unique_address_idx_mutex.unlock_shared();
    if(session_it == unique_address_idx.end()) return false;

    // Authorize new user
    UserEntry* new_user_entry = user_table.authorize(std::move(nickname), std::move(password_hash));
    if(!new_user_entry) return false;

    std::lock_guard lock(user_idx_mutex);

    // Remove old user from session index
    if(session_it->user_entry) {
      for(auto range = user_idx.equal_range(session_it->user_entry); range.first != range.second; ++range.first) {
        if((*range.first) == session_it) {
          user_idx.erase(range.first);
          break;
        }
      }
    }

    // Set new user to session and add session to index
    const_cast<UserEntry*&>(session_it->user_entry) = new_user_entry;
    const_cast<SessionEntry::Status&>(session_it->status) = SessionEntry::Status::authorized;
    user_idx.emplace(session_it);
    return true;
  }

  bool logout(TcpClientBase* socket) {
    unique_address_idx_mutex.lock_shared();
    auto session_it = unique_address_idx.find(stcp::TcpServer::ClientKey{.host = socket->getHost(), .port = socket->getPort()});
    unique_address_idx_mutex.unlock_shared();
    if(session_it == unique_address_idx.end()) return false;

    std::lock_guard lock(user_idx_mutex);

    // Remove old user from session index
    if(session_it->user_entry) {
      for(auto range = user_idx.equal_range(session_it->user_entry); range.first != range.second; ++range.first) {
        if((*range.first) == session_it) {
          user_idx.erase(range.first);
          break;
        }
      }
    } else return false;

    const_cast<UserEntry*&>(session_it->user_entry) = nullptr;
    const_cast<SessionEntry::Status&>(session_it->status) = SessionEntry::Status::unauthorized;
    return true;
  }

  std::list<TcpClientBase*> getSocketsByNickname(std::string nickname) {
    std::list<TcpClientBase*> socket_list;
    std::shared_lock lock(user_idx_mutex);
    UserEntry* user = user_table.find(nickname);
    if(!user) return {};
    for(auto range = user_idx.equal_range(user); range.first != range.second; ++range.first)
      socket_list.emplace_back((*range.first)->socket);
    return socket_list;
  }


} session_table;


enum class ActionCode : unsigned char {
  register_user = 0x00,
  authorize = 0x01,
  send_to = 0x02
};

enum class ResponseCode : unsigned char {
  auth_ok = 0x00,
  auth_fail = 0x01,
  incoming_message = 0x02,
  send_ok = 0x03,
  send_fail = 0x04,
  access_denied = 0xFF
};


//Parse ip to std::string
std::string getHostStr(const TcpServer::Client& client) {
    uint32_t ip = client.getHost ();
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( client.getPort ());
}

template<typename T>
T extract(DataBuffer::iterator& it) {
  T result = *reinterpret_cast<T*>(&*it);
  it += sizeof(T);
  return result;
}


std::string extractString(DataBuffer::iterator& it) {
  uint64_t string_size = extract<uint64_t>(it);
  std::string string(reinterpret_cast<std::string::value_type*>(&*it), string_size);
  it += string_size;
  return string;
}

template<typename T>
void append(DataBuffer& buffer, const T& data) {
  const uint8_t* data_it = reinterpret_cast<const uint8_t*>(&data);
  for(std::size_t data_size = sizeof(T); data_size; (--data_size, ++data_it))
    buffer.push_back(*data_it);
}

void appendString(DataBuffer& buffer, std::string_view str) {
  append<uint64_t>(buffer, str.size());
  for(const char& ch : str)
    buffer.push_back(reinterpret_cast<const uint8_t&>(ch));
}


TcpServer server(8081,
{1, 1, 1}, // Keep alive{idle:1s, interval: 1s, pk_count: 1}

[](DataBuffer data, TcpServer::Client& client){ // Data handler
  auto it = data.begin();
  uint16_t act_sequence = extract<uint16_t>(it);
  ActionCode act = extract<ActionCode>(it);

  switch (act) {

  case ActionCode::register_user: {
    std::string nickname = extractString(it);
    std::string password_hash = extractString(it);

    #pragma pack(push, 1)
    struct {
      uint16_t act_sequence;
      ResponseCode response_code;
    } response {
      act_sequence,
      session_table.registerUser(&client, std::move(nickname), std::move(password_hash))
      ? ResponseCode::auth_ok
      : ResponseCode::auth_fail
    };
    #pragma pack(pop)

    client.sendData(&response, sizeof(response));
    return;
  }

  case ActionCode::authorize: {
    std::string nickname = extractString(it);
    std::string password_hash = extractString(it);

    #pragma pack(push, 1)
    struct {
      uint16_t act_sequence;
      ResponseCode response_code;
    } response {
      act_sequence,
      session_table.authorize(&client, std::move(nickname), std::move(password_hash))
      ? ResponseCode::auth_ok
      : ResponseCode::auth_fail
    };
    #pragma pack(pop)

    client.sendData(&response, sizeof(response));
    return;
  }

  case ActionCode::send_to: {
    if(session_table.getStatus(&client) != SessionEntry::authorized) {
      #pragma pack(push, 1)
      struct {
        uint16_t act_sequence;
        ResponseCode response_code;
      } response {
        act_sequence,
        ResponseCode::access_denied
      };
      #pragma pack(pop)
      client.sendData(&response, sizeof(response));
      return;
    }

    std::string recipient_nickname = extractString(it);
    std::string sender_nickname = session_table.getNickname(&client);
    std::string message = extractString(it);
    auto recipient_sockets = session_table.getSocketsByNickname(recipient_nickname);

    if(recipient_sockets.empty()) {
      #pragma pack(push, 1)
      struct {
        uint16_t act_sequence;
        ResponseCode response_code;
      } response {
        act_sequence,
        ResponseCode::send_fail
      };
      #pragma pack(pop)
      client.sendData(&response, sizeof(response));
      return;
    }

    DataBuffer message_buffer;
    message_buffer.reserve(
      sizeof(uint16_t) + // act_sequence
      sizeof(ResponseCode) + // response_code
      sizeof(uint64_t) + // sender_nickname_size
      sender_nickname.size() + // sender_nickname
      sizeof(uint64_t) + // message_size
      message.size() // message
    );

    append(message_buffer, uint16_t(-1));
    append(message_buffer, ResponseCode::incoming_message);
    appendString(message_buffer, sender_nickname);
    appendString(message_buffer, message);

    for(auto& recipient_socket : recipient_sockets)
      recipient_socket->sendData(message_buffer.data(), message_buffer.size());

    #pragma pack(push, 1)
      struct {
        uint16_t act_sequence;
        ResponseCode response_code;
      } response {
        act_sequence,
        ResponseCode::send_ok
      };
      #pragma pack(pop)
      client.sendData(&response, sizeof(response));
      return;
  }

  }
},

[](TcpServer::Client& client) { // Connect handler
  std::cout << "Client " << getHostStr(client) << " connected\n";
  session_table.addSession(&client);
},

[](TcpServer::Client& client) { // Disconnect handler
  std::cout << "Client " << getHostStr(client) << " disconnected\n";
  session_table.logout(&client);
  session_table.removeSession(&client);
},

std::thread::hardware_concurrency() // Thread pool size
);



int main() {
  using namespace std::chrono_literals;
  try {
    //Start server
    if(server.start() == TcpServer::status::up) {
      std::cout<<"Server listen on port: " << server.getPort() << std::endl
               <<"Server handling thread pool size: " << server.getThreadPool().getThreadCount() << std::endl;
      server.joinLoop();
      return EXIT_SUCCESS;
    } else {
      std::cout<<"Server start error! Error code:"<< int(server.getStatus()) <<std::endl;
      return EXIT_FAILURE;
    }

  } catch(std::exception& except) {
    std::cerr << except.what();
    return EXIT_FAILURE;
  }
}
