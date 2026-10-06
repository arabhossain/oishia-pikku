#include "../learning/OishiaHttpServer.h"
#include <cassert>
#include <iostream>
OishiaHttpServer server(80);
int commands = 0;
void tick() { server.handleClient(); fakeMillis += 5; }
void finish() { for(int i=0; i<2000 && MockNetwork::connected; ++i) tick(); assert(!MockNetwork::connected); }
int main() {
  server.on("/api/command", HTTP_POST, [](){ ++commands; server.send(202,"application/json","{\"ok\":true}"); });
  static const uint8_t binary[] = {31,139,0,255,1};
  server.on("/", HTTP_GET, [](){ server.sendHeader("Content-Encoding","gzip"); server.send_P(200,"text/html",binary,sizeof(binary)); });
  server.begin();
  const std::string post="POST /api/command HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: 2\r\n\r\n";
  MockNetwork::accept(post+"{"); tick(); assert(commands==0);
  MockNetwork::input+="}"; finish(); assert(commands==1);
  assert(MockNetwork::output.find("202 Response")!=std::string::npos);
  assert(MockNetwork::output.find("Content-Length: 11\r\n")!=std::string::npos);
  MockNetwork::accept("GET / HTTP/1.1\r\n\r\n"); finish();
  auto body = MockNetwork::output.substr(MockNetwork::output.find("\r\n\r\n")+4);
  assert(body==std::string(reinterpret_cast<const char*>(binary),sizeof(binary)));
  MockNetwork::accept("POST /api/command HTTP/1.1\r\nContent-Length: 100000\r\n"); finish();
  assert(commands==1 && MockNetwork::output.find("413 Response")!=std::string::npos);
  // A trickling writer still reaches the absolute deadline.
  fakeMillis=0xfffffff0; MockNetwork::accept("G"); tick();
  fakeMillis=uint32_t(0xfffffff0u+2999); MockNetwork::input="E"; tick();
  fakeMillis=uint32_t(0xfffffff0u+3000); finish();
  assert(MockNetwork::output.find("408 Response")!=std::string::npos);
  // A client that never reads a response cannot hold the connection forever.
  MockNetwork::accept("GET / HTTP/1.1\r\n\r\n"); tick(); MockNetwork::blockWrites=true;
  finish(); assert(MockNetwork::output.empty());
  assert(MockNetwork::largestWrite<=512);
  std::cout<<"HTTP adapter bounds, fragmented requests, binary gzip, and slow RX/TX deadlines passed.\n";
}
