#include "exchange_state.hpp"

namespace eosio {

    extended_asset exchange_state::convert(extended_asset from, extended_symbol to_symbol) const {
        extended_asset out;

        if (from.symbol == base.symbol && to_symbol == quote.symbol) {
            out = extended_asset(from.amount * get_rprice(), to_symbol);
        } else if (from.symbol == quote.symbol && to_symbol == base.symbol) {
            out = extended_asset(from.amount * get_price(), to_symbol);
        } else {
            eosio_assert(false, "invalid conversion");
        }

        return out;
    }

    void exchange_state::print() const {
        eosio::print(
                name{manager}, ' ',
                base, "->",
                quote, ' ',
                get_price(), ' ',
                get_rprice(), ' ',
                primary_key()
        );
    }

} /// namespace eosio
