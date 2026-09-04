#ifndef ELECTRUM_LIBSPARKMOBILE_H
#define ELECTRUM_LIBSPARKMOBILE_H

#include <stdint.h>
#include "structs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SPARK_EXPORT
    #ifdef _WIN32
        #define SPARK_EXPORT __declspec(dllexport)
    #else
        #define SPARK_EXPORT __attribute__((visibility("default"))) __attribute__((used))
    #endif
#endif

SPARK_EXPORT
void* getFullViewKeyFromPrivateKeyData(unsigned char* keyData, int keyDataLength, int index);

SPARK_EXPORT
void* deserializeFullViewKey(unsigned char* keyData, int keyDataLength);

SPARK_EXPORT
unsigned char* serializeFullViewKey(void* fullViewKeyVoid, int* serializedSize);

SPARK_EXPORT
void deleteFullViewKey(void* fullViewKey);

SPARK_EXPORT
const char* getAddress(unsigned char* keyData, int keyDataLength, int index, int diversifier, int isTestNet);

SPARK_EXPORT
const char* getAddressFromFullViewKey(void* fullViewKeyVoid, int index, int diversifier, int isTestNet);

SPARK_EXPORT
struct AggregateCoinData* idAndRecoverCoin(
        const unsigned char* serializedCoin,
        int serializedCoinLength,
        unsigned char* keyData,
        int keyDataLength,
        int index,
        unsigned char* context,
        int contextLength,
        int isTestNet
);

SPARK_EXPORT
struct AggregateCoinData* idAndRecoverCoinByFullViewKey(
        const unsigned char* serializedCoin,
        int serializedCoinLength,
        void* fullViewKeyVoid,
        unsigned char* context,
        int contextLength,
        int isTestNet
);

SPARK_EXPORT
struct CCRecipientList* cCreateSparkMintRecipients(
        struct CMintedCoinData* outputs,
        int outputsLength,
        unsigned char* serial_context,
        int serial_contextLength,
        int generate,
        int isTestNet);

SPARK_EXPORT
struct SparkSpendTransactionResult* cCreateSparkSpendTransaction(
        unsigned char* keyData,
        int keyDataLength,
        int index,
        struct CRecip* recipients,
        int recipientsLength,
        struct COutputRecipient* privateRecipients,
        int privateRecipientsLength,
        struct SpendCoinData* coins,
        int coinsLength,
        struct CCoverSetData* cover_set_data_all,
        int cover_set_data_allLength,
        struct BlockHashAndId* idAndBlockHashes,
        int idAndBlockHashesLength,
        unsigned char* txHashSig,
        int txHashSigLength,
        unsigned char* extensionCommitment,
        int extensionCommitmentLength,
        int additionalTxSize,
        int isTestNet
);

SPARK_EXPORT
struct SerializedMintContextResult* serializeMintContext(
        struct TxInputData* inputs,
        int inputsLength
);

SPARK_EXPORT
struct ValidateAddressResult* isValidSparkAddress(
        const char* addressCStr,
        int isTestNet
);

SPARK_EXPORT
const char* hashTags(unsigned char* tags, int tagsLength, int tagCount);

SPARK_EXPORT
const char* hashTag(const char* x, const char* y);

SPARK_EXPORT
struct SparkFeeResult* estimateSparkFee(
        unsigned char* keyData,
        int keyDataLength,
        int index,
        int64_t sendAmount,
        int subtractFeeFromAmount,
        struct SpendCoinData* coins,
        int coinsLength,
        int privateRecipientsLength,
        int utxoNum,
        int additionalTxSize
);

SPARK_EXPORT
void native_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif
