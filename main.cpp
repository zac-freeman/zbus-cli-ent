#include "zbus-client.h"

#include <QCoreApplication>

int main(int argc, char **argv)
{
  QCoreApplication app(argc, argv);

  ZBusClient client("localhost:8180");
}
