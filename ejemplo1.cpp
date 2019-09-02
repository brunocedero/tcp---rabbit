#include <sys/socket.h>
#include <iostream>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include <stdio.h>
#include "amqpcpp.h"
#include "conn_handler.h"
//server
using namespace std;


int main(void)
{
//primer paso: crear un socket
int listening = socket(AF_INET, SOCK_STREAM, 0);
    if(listening == -1)
     {
          cerr << "No se ha podido crear el socket";

          return -1;
     }

//segundo paso: enlazar el socket para un ip / port
     sockaddr_in hint;
     hint.sin_family = AF_INET;
     hint.sin_port = htons(5400);
     inet_pton(AF_INET, "0.0.0.0", &hint.sin_addr);

     if(bind(listening, (sockaddr*)&hint, sizeof(hint)) == -1)
     {
          cerr << "No se pudo enlazar para IP/PORT";
          return -2;
     }

//tercer paso: marca el socket para escuchar
     if (listen(listening, SOMAXCONN) == -1)
     {
          cerr << "No se pude escuchar";
          return -3;
     }

//cuarto paso: aceptar una llamada 
     sockaddr_in client;
     socklen_t clientSize = sizeof(client);
     char host[NI_MAXHOST];
     char svc[NI_MAXSERV];
     int clientSocket = accept(listening, (sockaddr*)&client, &clientSize);

     if (clientSocket == -1)
     {
          cerr << "Problema con la conexion al cliente";
          return -4;
     } 

//cerrar la escucha del socket 
     close(listening);
     memset(host, 0, NI_MAXHOST);
     memset(svc, 0, NI_MAXSERV);

     int result = getnameinfo((sockaddr*)&client, sizeof(client), host, NI_MAXHOST, svc, NI_MAXSERV, 0);

     if(result)
     {
          cout << host << "conectado en" << svc << endl;
     }

     else 
     {
          inet_ntop(AF_INET, &client.sin_addr, host, NI_MAXHOST);
          cout << host << "conectado en " << ntohs(client.sin_port) << endl;
     }

//mientras recibe el mensaje, echo mensaje
     char buf[4096];
     while(true)
     {
          //limpiar el bufer
          memset(buf, 0, 4096);
          //esperar un mensaje 
          int bytesRecv = recv(clientSocket, buf, 4096, 0);
          if (bytesRecv == -1)
          {
               cerr << "hubo un problema de coexion" << endl;
               break;
          }

          if (bytesRecv == 0)
          {
               cout << "cliente desconectado" << endl;
               break;
          }
          
          //mostrar mensaje
     // string mensaje="";
     // mensaje = string(buf, 0, bytesRecv); 

     char mensaje[1000];
     memcpy((void*)mensaje, buf, bytesRecv);

     auto evbase = event_base_new();
    LibEventHandlerMyError hndl(evbase);

    AMQP::TcpConnection connection(&hndl, AMQP::Address("amqp://localhost/"));
    AMQP::TcpChannel channel(&connection);
    channel.onError([&evbase](const char* mensaje)
        {
            std::cout << "Channel error: " << mensaje << std::endl;
            event_base_loopbreak(evbase);
        });
    channel.declareQueue("hello", AMQP::passive)
        .onSuccess
        (
            [&connection](const std::string &name,
                          uint32_t messagecount,
                          uint32_t consumercount)
            {
                std::cout << "Queue: " << name << std::endl;
            }
        )
        .onFinalize
        (
            [&connection]()
            {
                std::cout << "Terminado." << std::endl;
                connection.close();
            }
        );
    channel.publish("", "hello", buf, bytesRecv, 0);

    event_base_dispatch(evbase);
    event_base_free(evbase);
          cout << "recibido: " << mensaje << endl;

          //reenviar mensaje 
          send(clientSocket, buf, bytesRecv+1, 0);
     }
     close(clientSocket);
}