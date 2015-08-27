/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

// Pointer conversion test.
// NOTE needs to be run with valgrind to be effective.


#include "test_bits.hpp"
#include "proton/connection.hpp"
#include "proton/session.hpp"
#include "proton/session.hpp"

using namespace std;
using namespace proton;

template <class connection_ptr, class session_ptr>
void test_shared() {
    connection_ptr conn(*connection::cast(pn_connection()));
    session& s = conn->default_session();
    session_ptr p = s;
    ASSERT(p.unique());
    session_ptr p2 = s;
    ASSERT(!p.unique());                      // Should be in same family as s
    conn.reset();                               // Make sure we still have session
    p->create_sender("");
}

template <class connection_ptr, class session_ptr>
void test_counted() {
    connection_ptr conn(connection::cast(pn_connection()), false);
    session& s = conn->default_session();
    session_ptr p = s;
    session_ptr p2 = s;
    conn.reset();                               // Make sure we still have session
    p->create_sender("");
}

template <class connection_ptr, class session_ptr>
void test_unique() {
    connection_ptr conn(connection::cast(pn_connection()));
    session& s = conn->default_session();
    session_ptr p(s);
    session_ptr p2(s);
    conn.reset();                               // Make sure we still have session
    p->create_sender("");
}


int main(int argc, char** argv) {
    int failed = 0;
    failed += run_test(test_counted<counted_ptr<connection>,
                       counted_ptr<session> >, "counted");
#if PN_CPP11
    failed += run_test(test_shared<std::shared_ptr<connection>,
                       std::shared_ptr<session> >, "std::shared");
    failed += run_test(test_unique<std::unique_ptr<connection>,
                       std::unique_ptr<session> >, "std::unique");
#endif
#if PN_USE_BOOST
    failed += run_test(test_shared<boost::shared_ptr<connection>,
                       boost::shared_ptr<session> >, "boost::shared");
    failed += run_test(test_counted<boost::intrusive_ptr<connection>,
                       boost::intrusive_ptr<session> >, "boost::intrusive");
#endif
    return failed;
}

