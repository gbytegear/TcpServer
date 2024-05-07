#include "tcp/include/TcpClient.h"

#include <iostream>
#include <stdlib.h>
#include <thread>
#include <list>

using namespace stcp;

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

std::string getHostStr(uint32_t ip, uint16_t port) {
    return std::string() + std::to_string(int(reinterpret_cast<char*>(&ip)[0])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[1])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[2])) + '.' +
            std::to_string(int(reinterpret_cast<char*>(&ip)[3])) + ':' +
            std::to_string( port );
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

ThreadPool thread_pool;
TcpClient client(&thread_pool);
uint16_t act_sequence = 0;

std::mutex auth_mutex;
std::condition_variable auth_cv;
uint16_t auth_act_sequence;
ResponseCode auth_response;

std::list<std::string> message_list;
std::string reciver_nickname;

ResponseCode waitAuth(uint16_t act_sequence) {
  std::unique_lock lk(auth_mutex);
  auth_cv.wait(lk, [&act_sequence]{ return auth_act_sequence == act_sequence; });
  return auth_response;
}

void clearConsole() {
#ifdef _WIN32 // Windows NT
  COORD topLeft  = { 0, 0 };
  HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO screen;
  DWORD written;

  GetConsoleScreenBufferInfo(console, &screen);
  FillConsoleOutputCharacterA(
      console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written
  );
  FillConsoleOutputAttribute(
      console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
      screen.dwSize.X * screen.dwSize.Y, topLeft, &written
  );
  SetConsoleCursorPosition(console, topLeft);
#else
  std::cout << "\x1B[2J\x1B[H";
#endif
}

void update() {
  clearConsole();
  for(const auto& message : message_list)
    std::cout << message << std::endl;
  std::cout << "Enter reciver nickname: ";
  if(!reciver_nickname.empty()) {
    std::cout << reciver_nickname << std::endl;
    std::cout << "Enter message: ";
  }
}

void reciveHandler(DataBuffer data) {
  auto data_it = data.begin();
  uint16_t act_sequence = extract<uint16_t>(data_it);
  ResponseCode response_code = extract<ResponseCode>(data_it);

  switch (response_code) {
  case ResponseCode::auth_ok:
  case ResponseCode::auth_fail:
  {
    std::unique_lock lk(auth_mutex);
    auth_act_sequence = act_sequence;
    auth_response = response_code;
  }
  auth_cv.notify_one();
  return;

  case ResponseCode::incoming_message: {
    std::string sender_nickname = extractString(data_it);
    std::string message = extractString(data_it);
    message_list.push_back(sender_nickname + ": " + message);
    update();
    return;
  }

  case ResponseCode::send_ok: return;
  case ResponseCode::send_fail: {
    message_list.push_back("Send message fail!");
    update();
    return;
  }
  case ResponseCode::access_denied:
  return;
  }
}


void cli() {
  update();
  std::getline(std::cin, reciver_nickname);
  update();
  std::string message;
  std::getline(std::cin, message);

  DataBuffer buffer;
  buffer.reserve(
    sizeof(uint16_t) + // act_sequence
    sizeof(ActionCode) + // act_code
    sizeof(uint64_t) + // reciver_nickname_size
    reciver_nickname.size() + // reciver_nickname
    sizeof(uint64_t) + // message_size
    message.size() // message
  );
  append(buffer, act_sequence++);
  append(buffer, ActionCode::send_to);
  appendString(buffer, reciver_nickname);
  appendString(buffer, message);

  client.sendData(buffer.data(), buffer.size());
  message_list.push_back(std::string("You: ") + message);
  reciver_nickname.clear();

  thread_pool.addJob(cli);
}

void auth() {
  do {
    std::string input;
    std::clog << "Select action:\n"
                 "1. Authentication\n"
                 "2. Registration\n"
                 "> ";
    int action_number;
    std::getline(std::cin, input);
    action_number = std::stoi(input);

    if(action_number > 2 || action_number < 1) continue;
    std::string login, password;

    std::clog << "Login: "; std::getline(std::cin, login);
    std::clog << "Password: "; std::getline(std::cin, password);

    DataBuffer buffer;
    buffer.reserve(
      sizeof(uint16_t) + // act_sequence
      sizeof(ActionCode) + // act_code
      sizeof(uint64_t) + // login_size
      login.size() + // login
      sizeof(uint64_t) + // password_size
      password.size() // password
    );
    append(buffer, act_sequence);
    append(buffer, action_number == 1 ? ActionCode::authorize : ActionCode::register_user);
    appendString(buffer, login);
    appendString(buffer, password);

    client.sendData(buffer.data(), buffer.size());
    if(waitAuth(act_sequence++) == ResponseCode::auth_ok) break;
    std::cerr << "Authentication failed!\n";

  } while(true);

  thread_pool.addJob(cli);
}



int main(int, char**) {
  if(client.connectTo(LOCALHOST_IP, 8081) == SocketStatus::connected) {
    std::clog << "Client connected\n";
    thread_pool.addJob(auth);
    client.setHandler(reciveHandler);
    thread_pool.join();
  } else {
    std::cerr << "Client isn't connected\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
