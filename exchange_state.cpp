#include "exchange_state.hpp"

namespace eosio {

    uint64_t pow10(uint64_t power) {
        uint64_t result = 1;
        for (uint64_t i = 0; i < power; i++) {
            result *= 10;
        }
        return result;
    }

    extended_asset exchange_state::convert(extended_asset from, extended_symbol to_symbol) const {
        uint64_t out;

        if (from.symbol == base.symbol && to_symbol == quote.symbol) {
            out = from.amount * get_price();
        } else if (from.symbol == quote.symbol && to_symbol == base.symbol) {
            out = from.amount * get_rprice();
        } else {
            eosio_assert(false, "invalid conversion");
        }

        return extended_asset(out * (pow10(to_symbol.precision()) / pow10(from.symbol.precision())), to_symbol);
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
