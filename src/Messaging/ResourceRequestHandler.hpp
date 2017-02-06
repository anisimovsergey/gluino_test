// Copyright Sergey Anisimov 2016-2017
// MIT License
//
// Gluino
// https://github.com/anisimovsergey/gluino

#ifndef MESSAGING_RESOURCE_REQUEST_HANDLER_HPP
#define MESSAGING_RESOURCE_REQUEST_HANDLER_HPP

#include "Request.hpp"

#include <functional>

namespace Messaging {

  struct ResourceRequestHandler {
    TYPE_PTRS_ABSTRACT(ResourceRequestHandler)
    virtual ~ResourceRequestHandler() = default;
    virtual std::string getRequestType() const = 0;
    virtual std::string getContentType() const = 0;
    virtual Core::IEntity::Unique makeRequest(const Request& request) const = 0;
  };

  template <typename T, class = void>
  class ResourceRequestHandlerImpl;

  template<typename T>
  class ResourceRequestHandlerImpl<T,
    typename std::enable_if<Core::function_traits<T>::value == 0>::type> : public ResourceRequestHandler {
    public:
      ResourceRequestHandlerImpl(
        std::string requestType,
        T onRequest) :
        requestType(requestType),
        onRequest(onRequest) {
      }

      virtual std::string getRequestType() const override {
        return requestType;
      }

      virtual std::string getContentType() const override {
        return "";
      }

      virtual Core::IEntity::Unique makeRequest(const Request&) const override {
         return onRequest();
      }

    private:
      const std::string requestType;
      const T onRequest;
  };

  template<typename T>
  class ResourceRequestHandlerImpl<T,
    typename std::enable_if<Core::function_traits<T>::value == 1>::type> : public ResourceRequestHandler {
    typedef typename Core::function_traits<T> traits;

  public:
      ResourceRequestHandlerImpl(
        std::string requestType,
        T onRequest) :
        requestType(requestType),
        onRequest(onRequest) {
      }

      virtual std::string getRequestType() const override {
        return requestType;
      }

      virtual std::string getContentType() const override {
        return Core::base_type<typename traits::template arg<0>::type>::TypeId();
      }

      virtual Core::IEntity::Unique makeRequest(const Request& request) const override {
        return onRequest(static_cast<typename traits::template arg<0>::type>(*request.getContent()));
      }

    private:
      const std::string requestType;
      const T onRequest;
  };

}


#endif /* end of include guard: MESSAGING_RESOURCE_REQUEST_HANDLER_HPP */
