#include "MessageQueue.hpp"

#include "Core/Casting.hpp"

using namespace Core;
using namespace Messaging;

namespace {
  static const std::string messageQueueSenderId = "messageQueue";
}

MessageQueue::MessageQueue(ILogger& logger): logger(logger) {
}

void
MessageQueue::idle() {
  while (!requests.empty())
  {
    auto& request = requests.front();
    processRequest(*request);
    requests.pop();
  }
  while (!responses.empty())
  {
    auto& response = responses.front();
    processResponse(*response);
    responses.pop();
  }
  while (!events.empty()) {
    auto& event = events.front();
    processEvent(*event);
    events.pop();
  }
}

Status
MessageQueue::addRequest(Request::Unique request) {
  requests.emplace(std::move(request));
  return Status::OK;
}

Status
MessageQueue::addEvent(Event::Unique event) {
  events.emplace(std::move(event));
  return Status::OK;
}

QueueGenericClient::Unique
MessageQueue::createClient(std::string clientId) {
  auto client = QueueGenericClient::makeUnique(clientId, *this);
  clients.push_back(client.get());
  return client;
}

QueueResourceClient::Unique
MessageQueue::createClient(std::string clientId, std::string resource) {
  auto client = QueueResourceClient::makeUnique(clientId, resource, *this);
  clients.push_back(client.get());
  return client;
}

void
MessageQueue::removeClient(const QueueClient& client) {
  clients.remove(&client);
}

QueueResourceController::Unique
MessageQueue::createController(std::string resource) {
  auto controller = QueueResourceController::makeUnique(resource, *this);
  controllers.push_back(controller.get());
  return controller;
}

void
MessageQueue::removeController(const QueueResourceController& controller) {
  controllers.remove(&controller);
}

void
MessageQueue::processRequest(const Request& request) {
  logger.message("Processing a request from '" + request.getSender() + "'");
  IEntity::Unique result;
  auto handler = getRequestHandler(request);
  if (handler) {
    result = handler(request);
  } else {
    logger.error("Unable to find a request handler.");
    result = Status::makeUnique(StatusCode::NotFound, "Unable to find a request handler.");
  }
  sendResponseFor(request, std::move(result));
}

void
MessageQueue::processResponse(const Response& response) {
  auto receiver = response.getReceiver();
  logger.message("Processing a response to '" + receiver + "'");
  auto client = getClient(receiver);
  if (client) {
    client->onResponse(response);
  } else {
    logger.error("Unable to find client '" + receiver + "'");
  }
}

void
MessageQueue::processEvent(const Event& event) {
  logger.message("Broadcasting event '" + event.getEventType() + "'.");
  for(auto client: clients) {
	  client->onEvent(event);
  }
}

const QueueClient*
MessageQueue::getClient(std::string clientId) {
  for (auto& client : clients) {
    if (client->getClientId() == clientId) {
      return client;
    }
  }
  return nullptr;
}

RequestHandler
MessageQueue::getRequestHandler(const Request& request) {
  for(auto& controller: controllers) {
    auto handler = controller->getRequestHandler(request);
    if (handler) {
      return handler;
    }
  }
  return nullptr;
}

void
MessageQueue::sendResponseFor(const Request& request, IEntity::Unique result) {
  auto response = Response::makeUnique(
		  request.getSender(),
          request.getRequestType(),
          request.getResource(),
          std::move(result)
         );

  responses.emplace(std::move(response));
}
