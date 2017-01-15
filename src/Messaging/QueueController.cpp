#include "QueueController.hpp"
#include "IMessageQueue.hpp"

using namespace Core;
using namespace Messaging;

QueueController::QueueController(IMessageQueue& messageQueue) : messageQueue(messageQueue) {
}

StatusResult::Unique
QueueController::sendEvent(std::string type,
  std::string resource, IEntity::Shared result) {
  auto event = Event::makeShared(type, resource, result);
  return messageQueue.addEvent(event);
}

bool
QueueController::canProcessRequest(const Request& request) {
  if (canProcessRequestHandler)
    return canProcessRequestHandler(request);
  return false;
}

IEntity::Unique
QueueController::processRequest(const Request& request) {
  if (processRequestHandler)
    return processRequestHandler(request);
  return StatusResult::NotImplemented();
}
