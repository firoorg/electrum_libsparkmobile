#include "utils.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>
#include <string>

#include "structs.h"
#include "deps/sparkmobile/src/coin.h"
#include "deps/sparkmobile/src/keys.h"
#include "deps/sparkmobile/bitcoin/script.h"

spark::SpendKey createSpendKeyFromData(unsigned char *keyData, int index) {
    if (keyData == nullptr) {
        throw std::invalid_argument("spend key data must not be null");
    }
    SpendKeyData data(keyData, index);
    return createSpendKey(data);
}

spark::Coin coinFromCCDataStream(CCDataStream& cdStream) {
    spark::Coin coin = deserializeCoin(cdStream.data, cdStream.length);
    return coin;
}

CScript createCScriptFromBytes(const unsigned char* bytes, int length) {
    CScript script;

    if (bytes != nullptr && length > 0) {
        for (int i = 0; i < length; ++i) {
            script << bytes[i];
        }
    }

    return script;
}

std::vector<unsigned char> serializeCScript(const CScript& script) {
    return std::vector<unsigned char>(script.begin(), script.end());
}

bool isValidAddressPoint(const secp_primitives::GroupElement& point) {
    return point.isMember() && !point.isInfinity();
}

void requireValidAddressPoints(const spark::Address& address) {
    if (!isValidAddressPoint(address.get_Q1())
            || !isValidAddressPoint(address.get_Q2())) {
        throw std::invalid_argument(
                "spark address contains an unusable group element");
    }
}

spark::Address decodeAddress(const std::string& str) {
    spark::Address address;
    address.decode(str);
    requireValidAddressPoints(address);

    return address;
}

spark::Address decodeAddress(const std::string& str, unsigned char expectedNetwork) {
    spark::Address address;
    unsigned char network = address.decode(str);
    if (network != expectedNetwork) {
        throw std::invalid_argument("spark address is for a different network");
    }
    requireValidAddressPoints(address);

    return address;
}

spark::Coin deserializeCoin(const unsigned char *serializedCoin, int length) {
    if (serializedCoin == nullptr || length <= 0) {
        throw std::invalid_argument("serialized coin must not be empty");
    }
    if (length > kMaxSerializedCoinSize) {
        throw std::invalid_argument("serialized coin is too large");
    }
    const char* begin = reinterpret_cast<const char*>(serializedCoin);
    CDataStream stream(begin, begin + length, SER_NETWORK, PROTOCOL_VERSION);
    spark::Coin coin(spark::Params::get_default());
    stream >> coin;
    return coin;
}
