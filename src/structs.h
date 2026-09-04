#ifndef ELECTRUM_LIBSPARKMOBILE_STRUCTS_H
#define ELECTRUM_LIBSPARKMOBILE_STRUCTS_H

#include <stdint.h>

struct CCRecipient {
    unsigned char *pubKey;
    int pubKeyLength;
    uint64_t cAmount;
    int subtractFee;
};

struct CCRecipientList {
    struct CCRecipient* list;
    int length;
};

struct CMintedCoinData {
    const char *address;
    uint64_t value;
    const char *memo;
};

struct CRecip {
    uint64_t amount;
    int subtractFee;
};

struct COutputCoinData {
    const char *address;
    int addressLength;
    uint64_t value;
    const char *memo;
    int memoLength;
};

struct COutputRecipient {
    struct COutputCoinData* output;
    int subtractFee;
};

struct CCDataStream {
    unsigned char *data;
    int length;
};

struct CCoverSetData {
    struct CCDataStream *cover_set;
    int cover_setLength;
    const unsigned char *cover_set_representation;
    int cover_set_representationLength;

    int setId;
};

struct OutputScript {
    unsigned char *bytes;
    int length;
};

struct AggregateCoinData {
    char type;
    uint64_t diversifier;
    uint64_t value;
    char *address;
    char *memo;
    char *lTagHash;

    unsigned char *encryptedDiversifier;
    int encryptedDiversifierLength;

    unsigned char *serial;
    int serialLength;

    char *nonceHex;
    int nonceHexLength;
};

struct SparkSpendTransactionResult {
    unsigned char *data;
    int dataLength;

    struct OutputScript* outputScripts;
    int outputScriptsLength;

    struct UsedCoin* usedCoins;
    int usedCoinsLength;

    int64_t fee;

    int isError;
};

struct TxInputData {
    unsigned char *txHash;
    int txHashLength;

    int vout;
};

struct SerializedMintContextResult {
    unsigned char *context;
    int contextLength;
};

struct ValidateAddressResult {
    int isValid;
    char *errorMessage;
};

struct BlockHashAndId {
    unsigned char* hash;
    int hashLength;
    int id;
};

struct SpendCoinData {
    struct CCDataStream* serializedCoin;
    struct CCDataStream* serializedCoinContext;
    int groupId;
    int height;
};

struct SparkFeeResult {
    char *error;
    int64_t fee;
};

struct UsedCoin {
    struct CCDataStream* serializedCoin;
    struct CCDataStream* serializedCoinContext;
    int groupId;
    int height;
};

#endif
