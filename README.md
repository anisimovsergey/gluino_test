# Gluino
A tiny message bus library for processing requests and broadcasting evens for implementing IoT application components communication.

This library contains a very basic implementation of a message queue and serialization abstractions. Exceptions and RTTI are not used intentionally since they might not be supported by the compilers for IOT devices.

[![Build Status](https://api.travis-ci.org/anisimovsergey/gluino.svg?branch=master)](https://travis-ci.org/anisimovsergey/gluino?branch=master)
[![Build status](https://ci.appveyor.com/api/projects/status/oiyjkkvbiyfy2u0h?svg=true)](https://ci.appveyor.com/project/anisimovsergey/gluino)
[![Coverage Status](https://coveralls.io/repos/github/anisimovsergey/gluino/badge.svg?branch=master)](https://coveralls.io/github/anisimovsergey/gluino?branch=master)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/74ecfbf675f34a3192ee0894ba75043e)](https://www.codacy.com/app/anisimovsergey/gluino?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=anisimovsergey/gluino&amp;utm_campaign=Badge_Grade)

## How to build and test

Clone the library
```shell
$ git clone https://github.com/anisimovsergey/gluino.git
$ cd gluino
```

Build using cmake
```shell
$ mkdir build
$ cd build
$ cmake ..
$ make
```

Run the tests
```shell
$ ./test/gluino_test
```

Reset the counters
```shell
$ lcov --directory . --zerocounters
```

## Core components

### IEntity
Even though RTTI is not used in this library it is still possible to do the dynamic casting using a very trivial type identification system. In order to participate in this system your class should inherit from IEntity and use TYPE_INFO macro.

```cpp
class Color : public Core::IEntity {
  TYPE_INFO(Color, Core::IEntity, "color")
  public:
    Color(uint8_t r, uint8_t g, uint8_t b);
    ...
};
```

### Status class
Status class is used for propagating an operation result and contains a status code, a message and an optional nested status. In order to return the result of an operation with some payload you can construct a tuple.

```cpp
std::tuple<Core::Status, std::unique_ptr<Color>>
RequestSerializer::deserializeImpl(const IDeserializationContext& context) const {
  ...
  auto color = std::make_unique<Color>(r, g, b);
  return std::make_tuple(Status::OK, std::move(color));
}
```

## The message queue
The message queue implemented in the library helps to decouple components of your application by

All the messages in the system are divided into three types: requests, responses and events. Requests are added to the message queue and processed in FIFO order.

There are two major scenarios you can implement using the message queue.

1. Reading the resource.
The resource client is sending the read request to the message bus. The resource controller, which is responsible for managing a specified resource, is receiving the request and responds with the response message. Normally the response contains the resource as a payload but in case of an error the payload contains status with the error description. The read operation supposed to be idempotent and should not modify the resource.


2. Modifying the resource.
The resource modification can initiated by a resource client. A resource modification request gets added to the message queue and received but a resource controller. The resource controller modifies the resource and responds to the client with the response containing the operation result. When the resource is successfully modified the event describing the modified resource is also broadcasted to all clients.

### Request
The request message (Request class) is send by resource clients (QueueResourceClient class) and handled by resource controllers (QueueResourceController class).

```cpp
  auto connection = std::make_unique<ConnectionParams>("WIFI_NAME", "PASSWORD");
  auto request = std::make_unique<Request>("requestId", "clientId", RequestType::Create, "connection", std::move(connection));
```

### Response
The response message (Response class) is sent by queue resource controllers (QueueResourceController class) in the result of handling a request sent by a resource client (QueueResourceClient class).

```cpp
  auto status = std::make_unique<Status>(StatusCode::Created, "The connection was created.");
  auto response = std::make_unique<Response>("requestId", "clientId", RequestType::Create, "connection", std::move(status));
```

### Events
The event message (Event class) is sent by queue resource controllers (QueueResourceController class) in order to notify all the queue resource clients about some changes in the resource state. Normally an event is sent in the result of some successfully performed modification request.

```cpp
  auto connection = std::make_unique<Connection>("WIFI_NAME", isConnected);
  auto event = std::make_unique<Event>(EventType::Created, "connection", std::move(connection));
```

### Queue resource client
The queue resource client can be used to get access to a resource and perform operations with it. A resource client is created using IMessageQueue.createClient. During its creation the client gets added to the resource clients list of the queue and automatically removed when it goes out of scope.

```cpp
  colorClient = messageQueue.createClient("SenderId", Color::TypeId());
  colorClient->addOnResponse(RequestType::Read, [=](const Color& color) {
    onColorReadResponse(color);
  });
  colorClient->addOnEvent(EventType::Updated, [=](const Color& color) {
    onColorUpdatedEvent(color);
  });
```

The queue resource client simplifies the request sending by automatically including the client id and the resource type to the requests.

```cpp
  auto newColor = std::make_unique<Color>(r, g, b);
  colorClient->sendRequest(RequestType::Update, std::move(newColor));
```

### Queue resource controllers
The queue resource controller is handling requests to a particular resource. The requests can involve a modification of the resource or can be just simple read. In the former case the response should contain only the operation status and the new (modified) resource representation can be sent in the correspondent notification or explicitly retrieved by an idempotent read request.

```cpp
  colorController = messageQueue.createController(Color::TypeId());
  colorController->addOnRequest(RequestType::Update, [=](const Color& color){
    setColor(color);
    colorController->sendEvent(EventType::Updated, std::make_unique<Color>(color));
    return std::make_unique<Status>(Status::OK);
  });
```

In case of a read request the response should contain the resource representation if the request was successful or a status otherwise.

```cpp
  connectionController = messageQueue.createController(Connection::TypeId());
  connectionController->addOnRequest(RequestType::Read, [=](){
    if (hasConnection()) {
      return createConnectionObject();
    } else {
      return std::make_unique<Status>(StatusCode::NotFound, "The connection doesn't exist.");
    }
  });
```

### Serialization
Gluino library provides a very basic abstractions for serialization and deserialization. In order to use those abstractions you need to implement the following interfaces for your platform:

1. IContextFactory - a factory responsible for creating the serialization and deserialization contexts
* ISerializationContext - the context needed for serialization and contains methods like `setString`, `setInt` etc.
* IDeserializationContext - the context needed for deserialization and contains methods like `getString`, `getInt` etc.

In order to create a serializer for your object you need to inherit from `Seializer<T>` and implement `serializeImpl` and/or `deserializeImpl`.

```cpp
Core::Status
ColorSerializer::serializeImpl(ISerializationContext& context, const Color& status) const {
  auto result = context.setByte("red", color.getR());
  if (!result.isOk())
    return result;
  ... // Repeat for green and blue
  return Status::OK;
}

std::tuple<Core::Status, std::unique_ptr<Color>>
RequestSerializer::deserializeImpl(const IDeserializationContext& context) const {
  Status result;
  uint8_t r, g, b;
  std::tie(result, r) = context.getByte("r");
  if (!result.isOk())
    return std::make_tuple(result, nullptr);
  ... // Repeat for green and blue
  auto color = std::make_unique<Color>(r, g, b);
  return std::make_tuple(Status::OK, std::move(color));
}
```
