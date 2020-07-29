//
// connection_manager.hpp
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_CONNECTION_MANAGER_HPP
#define HTTP_CONNECTION_MANAGER_HPP

#include "connection.hpp"
#include <opendatacon/asio.h>
#include <set>

namespace http {

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class connection_manager
{
private:
  /// The io_context used to perform asynchronous operations.
  std::shared_ptr<odc::asio_service> pIOS = odc::asio_service::Get();

  /// The strand used to synchronise access to the connection set below
  std::unique_ptr<asio::io_service::strand> pSetStrand = pIOS->make_strand();

public:
  connection_manager(const connection_manager&) = delete;
  connection_manager& operator=(const connection_manager&) = delete;

  /// Construct a connection manager.
  connection_manager();

  /// Synchronised versions of their private couterparts
  std::function<void(const connection_ptr&)> start = pSetStrand->wrap([this](const connection_ptr& c){start_(c);});
  std::function<void(const connection_ptr&)> stop = pSetStrand->wrap([this](const connection_ptr& c){stop_(c);});
  std::function<void()> stop_all = pSetStrand->wrap([this](){stop_all_();});

private:
  /// Add the specified connection to the manager and start it.
  void start_(const connection_ptr& c);

  /// Stop the specified connection.
  void stop_(const connection_ptr& c);

  /// Stop all connections.
  void stop_all_();

  /// The managed connections.
  std::set<connection_ptr> connections_;
};

} // namespace http

#endif // HTTP_CONNECTION_MANAGER_HPP
