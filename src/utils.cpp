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
    if (index < 0) {
        throw std::invalid_argument("spend key index must not be negative");
    }
    SpendKeyData data(keyData, index);
    return createSpendKey(data);
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

namespace {

uint64_t readCompactSize(const unsigned char* data, size_t length, size_t* pos) {
    if (*pos >= length) {
        throw std::invalid_argument("serialized coin is truncated");
    }
    unsigned char first = data[(*pos)++];
    size_t width = first < 253 ? 0 : first == 253 ? 2 : first == 254 ? 4 : 8;
    if (width == 0) {
        return first;
    }
    if (length - *pos < width) {
        throw std::invalid_argument("serialized coin is truncated");
    }
    uint64_t value = 0;
    for (size_t i = 0; i < width; i++) {
        value |= static_cast<uint64_t>(data[*pos + i]) << (8 * i);
    }
    *pos += width;
    return value;
}

void requireCoinLengthsFit(const unsigned char* data, size_t length) {
    if (length < 1) {
        throw std::invalid_argument("serialized coin is truncated");
    }
    const char type = static_cast<char>(data[0]);
    if (type != spark::COIN_TYPE_MINT && type != spark::COIN_TYPE_SPEND) {
        throw std::invalid_argument("serialized coin has a bad type");
    }
    const size_t memoBytes = spark::Params::get_default()->get_memo_bytes();
    const size_t mintCiphertext = (1 + AES_BLOCKSIZE)
            + spark::SCALAR_ENCODING + (1 + memoBytes + 1);
    const uint64_t expected[3] = {
        type == spark::COIN_TYPE_MINT ? mintCiphertext : 8 + mintCiphertext,
        spark::AEAD_TAG_SIZE,
        spark::AEAD_COMMIT_SIZE,
    };
    size_t pos = 1 + 3 * secp_primitives::GroupElement::serialize_size;
    if (length < pos) {
        throw std::invalid_argument("serialized coin is truncated");
    }
    for (int i = 0; i < 3; i++) {
        uint64_t size = readCompactSize(data, length, &pos);
        if (size != expected[i]) {
            throw std::invalid_argument("serialized coin has a bad field size");
        }
        if (size > length - pos) {
            throw std::invalid_argument("serialized coin is truncated");
        }
        pos += static_cast<size_t>(size);
    }
    if (type == spark::COIN_TYPE_MINT && length - pos < sizeof(uint64_t)) {
        throw std::invalid_argument("serialized coin is truncated");
    }
}

}

spark::Coin deserializeCoin(const unsigned char *serializedCoin, int length) {
    if (serializedCoin == nullptr || length <= 0) {
        throw std::invalid_argument("serialized coin must not be empty");
    }
    if (length > kMaxSerializedCoinSize) {
        throw std::invalid_argument("serialized coin is too large");
    }
    requireCoinLengthsFit(serializedCoin, static_cast<size_t>(length));
    const char* begin = reinterpret_cast<const char*>(serializedCoin);
    CDataStream stream(begin, begin + length, SER_NETWORK, PROTOCOL_VERSION);
    spark::Coin coin(spark::Params::get_default());
    stream >> coin;
    return coin;
}
