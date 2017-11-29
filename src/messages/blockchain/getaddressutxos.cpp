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

#include <bitprim/rpc/messages/blockchain/getaddressdeltas.hpp>
#include <bitprim/rpc/messages/error_codes.hpp>
#include <bitprim/rpc/messages/utils.hpp>
#include <boost/thread/latch.hpp>

namespace bitprim {


bool json_in_getaddressutxos(nlohmann::json const& json_object, std::vector<std::string>& payment_address, bool& chain_info){
    // Example:
    //  curl --user bitcoin:local321 --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getaddressutxos", "params": [{"addresses": ["2N8Cdq7BcCCqb8KiA3HzYaGDLJ6tXmVjcZ1"], "chainInfo":true}] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/

    if (json_object["params"].size() == 0)
        return false;

    chain_info = false;

    auto temp = json_object["params"][0];
    if (temp.is_object()){
        if(!temp["chainInfo"].is_null()){
            chain_info  = temp["chainInfo"];
        }

        for (const auto & addr : temp["addresses"]){
            payment_address.push_back(addr);
        }
    } else {
        //Only one address:
        payment_address.push_back(json_object["params"][0]);
    }

    return true;
}

bool getaddressutxos (nlohmann::json& json_object, int& error, std::string& error_code, std::vector<std::string> const& payment_addresses, const bool chain_info, libbitcoin::blockchain::block_chain const& chain){
    boost::latch latch(2);
    nlohmann::json utxos;

    int i = 0;
    for(const auto & payment_address : payment_addresses) {
        libbitcoin::wallet::payment_address address(payment_address);
        if(address)
        {
            chain.fetch_history(address, INT_MAX, 0, [&](const libbitcoin::code &ec,
                                                         libbitcoin::chain::history_compact::list history_compact_list) {
                if (ec == libbitcoin::error::success) {
                    for(const auto & history : history_compact_list) {
                        if (history.kind == libbitcoin::chain::point_kind::output) {
                            // It's outpoint
                            boost::latch latch2(2);
                            chain.fetch_spend(history.point, [&](const libbitcoin::code &ec, libbitcoin::chain::input_point input) {
                                if (ec == libbitcoin::error::not_found) {
                                    // Output not spent
                                    utxos[i]["address"] = address.encoded();
                                    utxos[i]["txid"] = libbitcoin::encode_hash(history.point.hash());
                                    utxos[i]["outputIndex"] = history.point.index();
                                    utxos[i]["satoshis"] = history.value;
                                    utxos[i]["height"] = history.height;
                                    // We need to fetch the txn to get the script
                                    boost::latch latch3(2);
                                    chain.fetch_transaction(history.point.hash(), false,
                                                            [&](const libbitcoin::code &ec, libbitcoin::transaction_const_ptr tx_ptr, size_t index,
                                                                size_t height) {
                                                                if (ec == libbitcoin::error::success) {
                                                                    utxos[i]["script"] = libbitcoin::encode_base16(tx_ptr->outputs().at(history.point.index()).script().to_data(0));
                                                                } else {
                                                                    utxos[i]["script"] = "";
                                                                }
                                                                latch3.count_down();
                                    });
                                    latch3.count_down_and_wait();
                                    ++i;
                                }
                                latch2.count_down();
                            });
                            latch2.count_down_and_wait();
                        }
                    }
                } else {
                    error = bitprim::RPC_INVALID_ADDRESS_OR_KEY;
                    error_code = "No information available for address " + address;
                }
                latch.count_down();
            });
            latch.count_down_and_wait();
        } else {
            error = bitprim::RPC_INVALID_ADDRESS_OR_KEY;
            error_code = "Invalid address";
        }
    }

    if (chain_info) {
        json_object["utxos"] = utxos;

        size_t height;
        boost::latch latch2(2);
        chain.fetch_last_height([&] (const libbitcoin::code &ec, size_t last_height){
            if (ec == libbitcoin::error::success) {
                height = last_height;
            }
            latch2.count_down();
        });
        latch2.count_down_and_wait();

        boost::latch latch3(2);
        chain.fetch_block(height, [&] (const libbitcoin::code &ec, libbitcoin::block_const_ptr block, size_t){
            if (ec == libbitcoin::error::success) {
                json_object["height"] = height;
                json_object["hash"] = libbitcoin::encode_hash(block->hash());
            }
            latch3.count_down();
        });
        latch3.count_down_and_wait();

    } else {
        json_object = utxos;
    }

    if(error !=0)
        return false;

    return true;
}

nlohmann::json process_getaddressutxos(nlohmann::json const& json_in, libbitcoin::blockchain::block_chain const& chain, bool use_testnet_rules)
{

    nlohmann::json container, result;
    container["id"] = json_in["id"];

    int error = 0;
    std::string error_code;

    std::vector<std::string> payment_address;
    bool chain_info;
    if (!json_in_getaddressutxos(json_in, payment_address, chain_info)) 
    {
        //load error code
        //return
    }

    if (getaddressutxos(result, error, error_code, payment_address, chain_info, chain))
    {
        container["result"] = result;
        container["error"];
    } else {
        container["error"]["code"] = error;
        container["error"]["message"] = error_code;
    }

    return container;

}

}
