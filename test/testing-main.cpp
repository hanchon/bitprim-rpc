/**
* Copyright (c) 2017 Bitprim developers (see AUTHORS)
*
* This file is part of bitprim-node.
*
* bitprim-node is free software: you can redistribute it and/or
* modify it under the terms of the GNU Affero General Public License with
* additional permissions to the one published by the Free Software
* Foundation, either version 3 of the License, or (at your option)
* any later version. For more information see LICENSE.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU Affero General Public License for more details.
*
* You should have received a copy of the GNU Affero General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <bitcoin/blockchain.hpp>
#include <bitprim/rpc/zmq/zmq_helper.hpp>

int main () {
    libbitcoin::threadpool t(0);
    const libbitcoin::blockchain::settings s;
    const libbitcoin::database::settings d;
    libbitcoin::blockchain::block_chain chain(t, s, d);

    bitprim::rpc::zmq zmq_object(5556, chain);

    size_t i = 1;
    while (i<=5){
        if (zmq_object.send_random_data()){
            std::cout << "Message sent" << i << std::endl;
        }
        ++i;
    }

    zmq_object.close();

    std::cout << "program ended" << std::endl;
    return 1;
}