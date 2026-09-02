#include "electrum_libsparkmobile.h"
#include "utils.h"
#include "deps/sparkmobile/include/spark.h"
#include "deps/sparkmobile/src/sparkname.h"
#include "deps/sparkmobile/bitcoin/uint256.h"
#include "structs.h"
#include "transaction.h"
#include "deps/sparkmobile/bitcoin/script.h"
#include "deps/sparkmobile/bitcoin/serialize.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

using namespace spark;

namespace {

constexpr int kSpendKeyDataSize = 32;
constexpr int kUint256Size = 32;
constexpr int kSparkTagSize = 34;
constexpr int kLTagHashHexSize = 64;

constexpr int kMaxAbiCount = 1 << 20;
constexpr size_t kMaxAbiBytes = 256u * 1024u * 1024u;

const char* const kUnknownError = "unknown error";

bool validCount(int count) {
    return count >= 0 && count <= kMaxAbiCount;
}

bool validBuffer(const void* ptr, int length) {
    if (length < 0 || static_cast<size_t>(length) > kMaxAbiBytes) {
        return false;
    }
    return length == 0 || ptr != nullptr;
}

bool validFixedBuffer(const void* ptr, int length, int expected) {
    return ptr != nullptr && length == expected;
}

void* abiAlloc(size_t count, size_t elemSize) {
    if (elemSize != 0 && count > kMaxAbiBytes / elemSize) {
        return nullptr;
    }
    size_t total = count * elemSize;
    if (total > kMaxAbiBytes) {
        return nullptr;
    }
    return calloc(1, total != 0 ? total : 1);
}

template <typename T>
T* abiAllocArray(size_t count) {
    return static_cast<T*>(abiAlloc(count, sizeof(T)));
}

char* abiStrdup(const char* text) {
    if (text == nullptr) {
        text = "";
    }
    size_t length = strlen(text);
    char* copy = static_cast<char*>(abiAlloc(length + 1, 1));
    if (copy != nullptr) {
        memcpy(copy, text, length + 1);
    }
    return copy;
}

void freeAggregateCoinData(AggregateCoinData* data) {
    if (data == nullptr) {
        return;
    }
    free(data->address);
    free(data->memo);
    free(data->lTagHash);
    free(data->encryptedDiversifier);
    free(data->serial);
    free(data->nonceHex);
    free(data);
}

void freeRecipientList(CCRecipientList* list) {
    if (list == nullptr) {
        return;
    }
    if (list->list != nullptr) {
        for (int i = 0; i < list->length; i++) {
            free(list->list[i].pubKey);
        }
        free(list->list);
    }
    free(list);
}

void freeSpendResult(SparkSpendTransactionResult* result) {
    if (result == nullptr) {
        return;
    }
    if (result->usedCoins != nullptr) {
        for (int i = 0; i < result->usedCoinsLength; i++) {
            if (result->usedCoins[i].serializedCoin != nullptr) {
                free(result->usedCoins[i].serializedCoin->data);
                free(result->usedCoins[i].serializedCoin);
            }
            if (result->usedCoins[i].serializedCoinContext != nullptr) {
                free(result->usedCoins[i].serializedCoinContext->data);
                free(result->usedCoins[i].serializedCoinContext);
            }
        }
        free(result->usedCoins);
    }
    if (result->outputScripts != nullptr) {
        for (int i = 0; i < result->outputScriptsLength; i++) {
            free(result->outputScripts[i].bytes);
        }
        free(result->outputScripts);
    }
    free(result->data);
    free(result);
}

SparkSpendTransactionResult* spendError(const char* message) {
    SparkSpendTransactionResult* result = abiAllocArray<SparkSpendTransactionResult>(1);
    if (result == nullptr) {
        return nullptr;
    }
    result->isError = 1;
    if (message == nullptr) {
        message = kUnknownError;
    }
    size_t length = strlen(message);
    result->data = static_cast<unsigned char*>(abiAlloc(length + 1, 1));
    if (result->data != nullptr) {
        memcpy(result->data, message, length);
        result->dataLength = static_cast<int>(length);
    }
    return result;
}

bool validSpendCoins(const SpendCoinData* coins, int coinsLength) {
    if (!validCount(coinsLength)) {
        return false;
    }
    if (coinsLength > 0 && coins == nullptr) {
        return false;
    }
    for (int i = 0; i < coinsLength; i++) {
        if (coins[i].serializedCoin == nullptr
                || coins[i].serializedCoinContext == nullptr) {
            return false;
        }
        if (!validBuffer(coins[i].serializedCoin->data, coins[i].serializedCoin->length)
                || coins[i].serializedCoin->length <= 0) {
            return false;
        }
        if (!validBuffer(coins[i].serializedCoinContext->data,
                         coins[i].serializedCoinContext->length)) {
            return false;
        }
    }
    return true;
}

unsigned char addressNetwork(int isTestNet) {
    return isTestNet ? spark::ADDRESS_NETWORK_TESTNET : spark::ADDRESS_NETWORK_MAINNET;
}

}

#ifdef __cplusplus
extern "C" {
#endif

SPARK_EXPORT
const char* getAddress(unsigned char* keyData, int keyDataLength, int index, int diversifier, int isTestNet) {
    try {
        if (!validFixedBuffer(keyData, keyDataLength, kSpendKeyDataSize)) {
            return nullptr;
        }
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);

        spark::FullViewKey fullViewKey(spendKey);
        return getAddressFromFullViewKey(reinterpret_cast<void*>(&fullViewKey), index, diversifier, isTestNet);
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
const char* getAddressFromFullViewKey(void* fullViewKeyVoid, int index, int diversifier, int isTestNet) {
    try {
        if (fullViewKeyVoid == nullptr) {
            return nullptr;
        }
        spark::IncomingViewKey incomingViewKey(*static_cast<spark::FullViewKey*>(fullViewKeyVoid));
        spark::Address address(incomingViewKey, static_cast<uint64_t>(diversifier));

        std::string encodedAddress = address.encode(addressNetwork(isTestNet));

        return abiStrdup(encodedAddress.c_str());
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
void* getFullViewKeyFromPrivateKeyData(unsigned char* keyData, int keyDataLength, int index) {
    try {
        if (!validFixedBuffer(keyData, keyDataLength, kSpendKeyDataSize)) {
            return nullptr;
        }
        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        std::unique_ptr<spark::FullViewKey> fullViewKey(new spark::FullViewKey(spendKey));

        return static_cast<void*>(fullViewKey.release());
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
void* deserializeFullViewKey(unsigned char* keyData, int keyDataLength) {
    try {
        if (keyData == nullptr || keyDataLength <= 0
                || static_cast<size_t>(keyDataLength) > kMaxAbiBytes) {
            return nullptr;
        }

        std::unique_ptr<spark::FullViewKey> fullViewKey(
                new spark::FullViewKey(spark::Params::get_default()));

        size_t serializeSize = GetSerializeSize(*fullViewKey, SER_NETWORK, PROTOCOL_VERSION);
        if (serializeSize != static_cast<size_t>(keyDataLength)) {
            return nullptr;
        }

        CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
        s.write(reinterpret_cast<const char*>(keyData), keyDataLength);

        s >> *fullViewKey;

        return static_cast<void*>(fullViewKey.release());
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
unsigned char* serializeFullViewKey(void* fullViewKeyVoid, int* serializedSize) {
    if (serializedSize != nullptr) {
        *serializedSize = 0;
    }
    try {
        if (fullViewKeyVoid == nullptr || serializedSize == nullptr) {
            return nullptr;
        }
        spark::FullViewKey* fullViewKey = static_cast<spark::FullViewKey*>(fullViewKeyVoid);

        CDataStream s(SER_NETWORK, PROTOCOL_VERSION);
        s << *fullViewKey;

        unsigned char* result = static_cast<unsigned char*>(abiAlloc(s.size(), 1));
        if (result == nullptr) {
            return nullptr;
        }

        memcpy(result, s.data(), s.size());
        *serializedSize = static_cast<int>(s.size());

        return result;
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
void deleteFullViewKey(void* fullViewKey) {
    delete static_cast<spark::FullViewKey*>(fullViewKey);
}

SPARK_EXPORT
AggregateCoinData* idAndRecoverCoin(
        const unsigned char* serializedCoin,
        int serializedCoinLength,
        unsigned char* privateKeyData,
        int privateKeyDataLength,
        int index,
        unsigned char* context,
        int contextLength,
        int isTestNet) {
    void* fullViewKeyVoid = getFullViewKeyFromPrivateKeyData(
            privateKeyData, privateKeyDataLength, index);
    if (fullViewKeyVoid == nullptr) {
        return nullptr;
    }

    AggregateCoinData* result = nullptr;
    try {
        result = idAndRecoverCoinByFullViewKey(
            serializedCoin,
            serializedCoinLength,
            fullViewKeyVoid,
            context,
            contextLength,
            isTestNet
        );
    } catch (...) {
        result = nullptr;
    }

    try {
        deleteFullViewKey(fullViewKeyVoid);
    } catch (...) {

    }
    return result;
}

SPARK_EXPORT
AggregateCoinData* idAndRecoverCoinByFullViewKey(
        const unsigned char* serializedCoin,
        int serializedCoinLength,
        void* fullViewKeyVoid,
        unsigned char* context,
        int contextLength,
        int isTestNet) {
    AggregateCoinData* result = nullptr;
    try {
        if (fullViewKeyVoid == nullptr
                || !validBuffer(serializedCoin, serializedCoinLength)
                || serializedCoinLength <= 0
                || !validBuffer(context, contextLength)) {
            return nullptr;
        }

        spark::FullViewKey* fullViewKey = static_cast<spark::FullViewKey*>(fullViewKeyVoid);
        spark::Coin coin = deserializeCoin(serializedCoin, serializedCoinLength);

        std::vector<unsigned char> contextVec(context, context + contextLength);
        coin.setSerialContext(contextVec);

        spark::IncomingViewKey incomingViewKey(*fullViewKey);

        spark::IdentifiedCoinData identifiedCoinData = coin.identify(incomingViewKey);

        spark::RecoveredCoinData data = coin.recover(*fullViewKey, identifiedCoinData);

        spark::Address address = getAddress(incomingViewKey, identifiedCoinData.i);
        std::string addressString = address.encode(addressNetwork(isTestNet));

        const std::string lTagHex = primitives::GetLTagHash(data.T).GetHex();
        const std::string nonceHex = identifiedCoinData.k.GetHex();
        const std::string serialHex = data.s.GetHex();

        result = abiAllocArray<AggregateCoinData>(1);
        if (result == nullptr) {
            return nullptr;
        }

        result->type = coin.type;
        result->diversifier = identifiedCoinData.i;
        result->value = identifiedCoinData.v;

        result->address = abiStrdup(addressString.c_str());
        result->memo = abiStrdup(identifiedCoinData.memo.c_str());
        result->lTagHash = abiStrdup(lTagHex.c_str());
        result->encryptedDiversifier = static_cast<unsigned char*>(
                abiAlloc(identifiedCoinData.d.size(), 1));
        result->nonceHex = static_cast<char*>(abiAlloc(nonceHex.size(), 1));
        result->serial = static_cast<unsigned char*>(abiAlloc(serialHex.size(), 1));

        if (result->address == nullptr || result->memo == nullptr
                || result->lTagHash == nullptr
                || result->encryptedDiversifier == nullptr
                || result->nonceHex == nullptr || result->serial == nullptr) {
            freeAggregateCoinData(result);
            return nullptr;
        }

        result->encryptedDiversifierLength = static_cast<int>(identifiedCoinData.d.size());
        memcpy(result->encryptedDiversifier, identifiedCoinData.d.data(),
               identifiedCoinData.d.size());

        result->nonceHexLength = static_cast<int>(nonceHex.size());
        memcpy(result->nonceHex, nonceHex.c_str(), nonceHex.size());

        result->serialLength = static_cast<int>(serialHex.size());
        memcpy(result->serial, serialHex.c_str(), serialHex.size());

        return result;
    } catch (...) {
        freeAggregateCoinData(result);
        return nullptr;
    }
}

SPARK_EXPORT
CCRecipientList* cCreateSparkMintRecipients(
    struct CMintedCoinData* cOutputs,
    int outputsLength,
    unsigned char* serial_context,
    int serial_contextLength,
    int generate,
    int isTestNet
) {
    CCRecipientList* data = nullptr;
    try {
        if (cOutputs == nullptr || outputsLength <= 0 || !validCount(outputsLength)) {
            return nullptr;
        }
        if (!validBuffer(serial_context, serial_contextLength)) {
            return nullptr;
        }

        const unsigned char network = addressNetwork(isTestNet);

        std::vector<spark::MintedCoinData> outputs;
        outputs.reserve(static_cast<size_t>(outputsLength));

        for (int i = 0; i < outputsLength; i++) {
            if (cOutputs[i].address == nullptr) {
                return nullptr;
            }
            spark::MintedCoinData mintedCoinData;
            mintedCoinData.memo = cOutputs[i].memo != nullptr ? cOutputs[i].memo : "";

            mintedCoinData.address = decodeAddress(std::string(cOutputs[i].address), network);
            mintedCoinData.v = cOutputs[i].value;
            outputs.push_back(mintedCoinData);
        }

        std::vector<unsigned char> serialVec(serial_context, serial_context + serial_contextLength);

        std::vector<CRecipient> recipients = createSparkMintRecipients(outputs, serialVec, generate);

        data = abiAllocArray<CCRecipientList>(1);
        if (data == nullptr) {
            return nullptr;
        }
        data->list = abiAllocArray<CCRecipient>(recipients.size());
        if (data->list == nullptr) {
            free(data);
            return nullptr;
        }
        data->length = static_cast<int>(recipients.size());

        for (size_t i = 0; i < recipients.size(); i++) {
            std::vector<unsigned char> pubKey(recipients[i].pubKey.begin(),
                                              recipients[i].pubKey.end());
            data->list[i].cAmount = recipients[i].amount;
            data->list[i].subtractFee = recipients[i].subtractFeeFromAmount;
            data->list[i].pubKeyLength = static_cast<int>(pubKey.size());
            data->list[i].pubKey = static_cast<unsigned char*>(
                    abiAlloc(pubKey.size(), 1));
            if (data->list[i].pubKey == nullptr) {
                freeRecipientList(data);
                return nullptr;
            }
            memcpy(data->list[i].pubKey, pubKey.data(), pubKey.size());
        }

        return data;
    } catch (...) {
        freeRecipientList(data);
        return nullptr;
    }
}

SPARK_EXPORT
SparkSpendTransactionResult* cCreateSparkSpendTransaction(
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
    int additionalTxSize,
    int isTestNet
) {
    SparkSpendTransactionResult* result = nullptr;
    try {
        if (!validFixedBuffer(keyData, keyDataLength, kSpendKeyDataSize)) {
            return spendError("invalid spend key data");
        }
        if (!validFixedBuffer(txHashSig, txHashSigLength, kUint256Size)) {
            return spendError("tx hash signature must be 32 bytes");
        }
        if (!validCount(recipientsLength) || (recipientsLength > 0 && recipients == nullptr)
                || !validCount(privateRecipientsLength)
                || (privateRecipientsLength > 0 && privateRecipients == nullptr)
                || !validCount(cover_set_data_allLength)
                || (cover_set_data_allLength > 0 && cover_set_data_all == nullptr)
                || !validCount(idAndBlockHashesLength)
                || (idAndBlockHashesLength > 0 && idAndBlockHashes == nullptr)) {
            return spendError("invalid argument list");
        }
        if (coinsLength <= 0 || !validSpendCoins(coins, coinsLength)) {
            return spendError("invalid spend coins");
        }

        const unsigned char network = addressNetwork(isTestNet);

        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        std::vector<std::pair<CAmount, bool>> cppRecipients;
        for (int i = 0; i < recipientsLength; i++) {
            cppRecipients.push_back(std::make_pair(recipients[i].amount, recipients[i].subtractFee));
        }

        std::vector<std::pair<spark::OutputCoinData, bool>> cppPrivateRecipients;
        for (int i = 0; i < privateRecipientsLength; i++) {
            const COutputCoinData* output = privateRecipients[i].output;
            if (output == nullptr
                    || !validBuffer(output->address, output->addressLength)
                    || output->addressLength <= 0
                    || !validBuffer(output->memo, output->memoLength)) {
                return spendError("invalid private recipient");
            }
            spark::OutputCoinData outputCoinData;
            outputCoinData.memo = std::string(output->memo != nullptr ? output->memo : "",
                                              static_cast<size_t>(output->memoLength));
            outputCoinData.v = (uint64_t)output->value;
            std::string addrString(output->address, static_cast<size_t>(output->addressLength));
            outputCoinData.address = decodeAddress(addrString, network);
            cppPrivateRecipients.push_back(std::make_pair(outputCoinData, privateRecipients[i].subtractFee));
        }

        std::list<CSparkMintMeta> cppCoins;
        for (int i = 0; i < coinsLength; i++) {
            spark::Coin coin = deserializeCoin(coins[i].serializedCoin->data, coins[i].serializedCoin->length);
            std::vector<unsigned char> contextVec(coins[i].serializedCoinContext->data, coins[i].serializedCoinContext->data + coins[i].serializedCoinContext->length);
            coin.setSerialContext(contextVec);
            CSparkMintMeta meta = getMetadata(coin, incomingViewKey);
            meta.nId = coins[i].groupId;
            meta.nHeight = coins[i].height;
            meta.coin = coin;
            cppCoins.push_back(meta);
        }

        std::unordered_map<uint64_t, spark::CoverSetData> cppCoverSetDataAll;
        for (int i = 0; i < cover_set_data_allLength; i++) {
            if (!validCount(cover_set_data_all[i].cover_setLength)
                    || (cover_set_data_all[i].cover_setLength > 0
                        && cover_set_data_all[i].cover_set == nullptr)
                    || !validBuffer(cover_set_data_all[i].cover_set_representation,
                                    cover_set_data_all[i].cover_set_representationLength)) {
                return spendError("invalid cover set");
            }
            spark::CoverSetData cppCoverSetData;
            for (int j = 0; j < cover_set_data_all[i].cover_setLength; j++) {
                const CCDataStream& entry = cover_set_data_all[i].cover_set[j];
                if (!validBuffer(entry.data, entry.length) || entry.length <= 0) {
                    return spendError("invalid cover set coin");
                }
                spark::Coin coin = deserializeCoin(entry.data, entry.length);
                cppCoverSetData.cover_set.push_back(coin);
            }
            cppCoverSetData.cover_set_representation = std::vector<unsigned char>(cover_set_data_all[i].cover_set_representation, cover_set_data_all[i].cover_set_representation + cover_set_data_all[i].cover_set_representationLength);
            cppCoverSetDataAll[cover_set_data_all[i].setId] = cppCoverSetData;
        }

        std::map<uint64_t, uint256> cppIdAndBlockHashesAll;
        for (int i = 0; i < idAndBlockHashesLength; i++) {
            if (!validFixedBuffer(idAndBlockHashes[i].hash,
                                  idAndBlockHashes[i].hashLength, kUint256Size)) {
                return spendError("block hash must be 32 bytes");
            }
            std::vector<unsigned char> vec(idAndBlockHashes[i].hash,
                                           idAndBlockHashes[i].hash + kUint256Size);
            cppIdAndBlockHashesAll[idAndBlockHashes[i].id] = uint256(vec);
        }

        std::vector<unsigned char> vec(txHashSig, txHashSig + kUint256Size);
        uint256 cppTxHashSig = uint256(vec);

        std::vector<uint8_t> cppSerializedSpend;
        CAmount cppFee;
        std::vector<std::vector<unsigned char>> cppOutputScripts;

        std::vector<CSparkMintMeta> spentCoinsOut;

        createSparkSpendTransaction(
            spendKey,
            fullViewKey,
            incomingViewKey,
            cppRecipients,
            cppPrivateRecipients,
            cppCoins,
            cppCoverSetDataAll,
            cppIdAndBlockHashesAll,
            cppTxHashSig,
            additionalTxSize,
            cppFee,
            cppSerializedSpend,
            cppOutputScripts,
            spentCoinsOut
        );

        result = abiAllocArray<SparkSpendTransactionResult>(1);
        if (result == nullptr) {
            return spendError("out of memory");
        }
        result->isError = 0;
        result->fee = cppFee;

        result->usedCoins = abiAllocArray<UsedCoin>(spentCoinsOut.size());
        if (result->usedCoins == nullptr) {
            freeSpendResult(result);
            return spendError("out of memory");
        }
        result->usedCoinsLength = static_cast<int>(spentCoinsOut.size());
        for (size_t i = 0; i < spentCoinsOut.size(); i++) {
            result->usedCoins[i].height = spentCoinsOut[i].nHeight;
            result->usedCoins[i].groupId = spentCoinsOut[i].nId;

            CDataStream coinStream(SER_NETWORK, PROTOCOL_VERSION);
            coinStream << spentCoinsOut[i].coin;

            result->usedCoins[i].serializedCoin = abiAllocArray<CCDataStream>(1);
            result->usedCoins[i].serializedCoinContext = abiAllocArray<CCDataStream>(1);
            if (result->usedCoins[i].serializedCoin == nullptr
                    || result->usedCoins[i].serializedCoinContext == nullptr) {
                freeSpendResult(result);
                return spendError("out of memory");
            }

            result->usedCoins[i].serializedCoin->data =
                    static_cast<unsigned char*>(abiAlloc(coinStream.size(), 1));
            result->usedCoins[i].serializedCoinContext->data =
                    static_cast<unsigned char*>(abiAlloc(spentCoinsOut[i].serial_context.size(), 1));
            if (result->usedCoins[i].serializedCoin->data == nullptr
                    || result->usedCoins[i].serializedCoinContext->data == nullptr) {
                freeSpendResult(result);
                return spendError("out of memory");
            }

            result->usedCoins[i].serializedCoin->length = static_cast<int>(coinStream.size());
            memcpy(result->usedCoins[i].serializedCoin->data, coinStream.data(), coinStream.size());

            result->usedCoins[i].serializedCoinContext->length =
                    static_cast<int>(spentCoinsOut[i].serial_context.size());
            memcpy(result->usedCoins[i].serializedCoinContext->data,
                   spentCoinsOut[i].serial_context.data(),
                   spentCoinsOut[i].serial_context.size());
        }

        result->outputScripts = abiAllocArray<OutputScript>(cppOutputScripts.size());
        if (result->outputScripts == nullptr) {
            freeSpendResult(result);
            return spendError("out of memory");
        }
        result->outputScriptsLength = static_cast<int>(cppOutputScripts.size());
        for (size_t i = 0; i < cppOutputScripts.size(); i++) {
            result->outputScripts[i].bytes =
                    static_cast<unsigned char*>(abiAlloc(cppOutputScripts[i].size(), 1));
            if (result->outputScripts[i].bytes == nullptr) {
                freeSpendResult(result);
                return spendError("out of memory");
            }
            result->outputScripts[i].length = static_cast<int>(cppOutputScripts[i].size());
            memcpy(result->outputScripts[i].bytes, cppOutputScripts[i].data(),
                   cppOutputScripts[i].size());
        }

        result->data = static_cast<unsigned char*>(abiAlloc(cppSerializedSpend.size(), 1));
        if (result->data == nullptr) {
            freeSpendResult(result);
            return spendError("out of memory");
        }
        result->dataLength = static_cast<int>(cppSerializedSpend.size());
        memcpy(result->data, cppSerializedSpend.data(), cppSerializedSpend.size());

        return result;
    } catch (const std::exception& e) {
        freeSpendResult(result);
        return spendError(e.what());
    } catch (...) {
        freeSpendResult(result);
        return spendError(kUnknownError);
    }
}

SPARK_EXPORT
SerializedMintContextResult* serializeMintContext(
        TxInputData* inputs,
        int inputsLength
) {
    try {
        if (inputs == nullptr || inputsLength <= 0 || !validCount(inputsLength)) {
            return nullptr;
        }

        CDataStream serialContextStream(SER_NETWORK, PROTOCOL_VERSION);
        for (int i = 0; i < inputsLength; i++) {

            if (!validFixedBuffer(inputs[i].txHash, inputs[i].txHashLength, kUint256Size)) {
                return nullptr;
            }
            std::vector<unsigned char> vec(inputs[i].txHash, inputs[i].txHash + kUint256Size);
            CTxIn input(
                    uint256(vec),
                    inputs[i].vout,
                    CScript(),
                    std::numeric_limits<unsigned int>::max() - 1);
            input.scriptSig = CScript();
            input.scriptWitness.SetNull();
            serialContextStream << input;
        }

        SerializedMintContextResult* result = abiAllocArray<SerializedMintContextResult>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->context = static_cast<unsigned char*>(abiAlloc(serialContextStream.size(), 1));
        if (result->context == nullptr) {
            free(result);
            return nullptr;
        }
        result->contextLength = static_cast<int>(serialContextStream.size());
        memcpy(result->context, serialContextStream.data(), serialContextStream.size());
        return result;
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
ValidateAddressResult* isValidSparkAddress(
        const char* addressCStr,
        int isTestNet
) {
    ValidateAddressResult* result = abiAllocArray<ValidateAddressResult>(1);
    if (result == nullptr) {
        return nullptr;
    }
    try {
        if (addressCStr == nullptr) {
            result->errorMessage = abiStrdup("address must not be null");
            return result;
        }
        spark::Address address;
        std::string addressString(addressCStr);
        unsigned char network = address.decode(addressString);

        requireValidAddressPoints(address);
        result->isValid = network == addressNetwork(isTestNet);
        return result;
    } catch (const std::exception& e) {
        result->isValid = 0;
        result->errorMessage = abiStrdup(e.what());
        return result;
    } catch (...) {
        result->isValid = 0;
        result->errorMessage = abiStrdup(kUnknownError);
        return result;
    }
}

SPARK_EXPORT
const char* hashTags(unsigned char* tags, int tagsLength, int tagCount) {
    char* result = nullptr;
    try {
        if (tags == nullptr || tagCount <= 0 || !validCount(tagCount)) {
            return nullptr;
        }

        if (tagsLength != tagCount * kSparkTagSize) {
            return nullptr;
        }

        result = static_cast<char*>(
                abiAlloc(static_cast<size_t>(tagCount) * kLTagHashHexSize + 1, 1));
        if (result == nullptr) {
            return nullptr;
        }

        for (int i = 0; i < tagCount; i++) {
            secp_primitives::GroupElement tag;
            tag.deserialize(tags + (static_cast<size_t>(i) * kSparkTagSize));
            std::string hex = primitives::GetLTagHash(tag).GetHex();
            if (hex.size() != static_cast<size_t>(kLTagHashHexSize)) {
                free(result);
                return nullptr;
            }
            memcpy(result + (static_cast<size_t>(i) * kLTagHashHexSize), hex.c_str(),
                   kLTagHashHexSize);
        }
        return result;
    } catch (...) {
        free(result);
        return nullptr;
    }
}

SPARK_EXPORT
const char* hashTag(const char* x, const char* y) {
    try {
        if (x == nullptr || y == nullptr) {
            return nullptr;
        }
        secp_primitives::GroupElement tag = secp_primitives::GroupElement(x, y, 16);
        std::string hex = primitives::GetLTagHash(tag).GetHex();
        return abiStrdup(hex.c_str());
    } catch (...) {
        return nullptr;
    }
}

SPARK_EXPORT
SparkFeeResult* estimateSparkFee(
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
) {
    try {
        if (!validFixedBuffer(keyData, keyDataLength, kSpendKeyDataSize)
                || !validSpendCoins(coins, coinsLength)) {
            SparkFeeResult* result = abiAllocArray<SparkFeeResult>(1);
            if (result == nullptr) {
                return nullptr;
            }
            result->error = abiStrdup("invalid arguments");
            return result;
        }

        spark::SpendKey spendKey = createSpendKeyFromData(keyData, index);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        std::list<CSparkMintMeta> cppCoins;
        for (int i = 0; i < coinsLength; i++) {
            spark::Coin coin = deserializeCoin(coins[i].serializedCoin->data, coins[i].serializedCoin->length);
            std::vector<unsigned char> contextVec(coins[i].serializedCoinContext->data, coins[i].serializedCoinContext->data + coins[i].serializedCoinContext->length);
            coin.setSerialContext(contextVec);
            CSparkMintMeta meta = getMetadata(coin, incomingViewKey);
            meta.nId = coins[i].groupId;
            meta.nHeight = coins[i].height;
            meta.coin = coin;
            cppCoins.push_back(meta);
        }

        std::pair<CAmount, std::vector<CSparkMintMeta>> estimated = SelectSparkCoins(
                sendAmount,
                subtractFeeFromAmount > 0,
                cppCoins,
                privateRecipientsLength,
                utxoNum,
                additionalTxSize
        );

        SparkFeeResult* result = abiAllocArray<SparkFeeResult>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->error = nullptr;
        result->fee = estimated.first;

        return result;
    } catch (const std::exception& e) {
        SparkFeeResult* result = abiAllocArray<SparkFeeResult>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->fee = 0;
        result->error = abiStrdup(e.what());

        return result;
    } catch (...) {
        SparkFeeResult* result = abiAllocArray<SparkFeeResult>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->fee = 0;
        result->error = abiStrdup(kUnknownError);

        return result;
    }
}

SPARK_EXPORT
SparkNameScript* createSparkNameScript(
        int sparkNameValidityBlocks,
        const char* name,
        const char* additionalInfo,
        const char* scalarMHex,
        unsigned char* spendKeyData,
        int spendKeyDataLength,
        int spendKeyIndex,
        int diversifier,
        int isTestNet,
        int hashFailSafe,
        int withoutProof
) {
    try {
        if (!validFixedBuffer(spendKeyData, spendKeyDataLength, kSpendKeyDataSize)
                || name == nullptr || additionalInfo == nullptr
                || (!withoutProof && scalarMHex == nullptr)) {
            SparkNameScript* result = abiAllocArray<SparkNameScript>(1);
            if (result == nullptr) {
                return nullptr;
            }
            result->error = abiStrdup("invalid arguments");
            return result;
        }

        spark::SpendKey spendKey = createSpendKeyFromData(spendKeyData, spendKeyIndex);
        spark::FullViewKey fullViewKey(spendKey);
        spark::IncomingViewKey incomingViewKey(fullViewKey);

        std::string nameString(name);
        std::string infoString(additionalInfo);

        spark::CSparkNameTxData nameTxData;
        nameTxData.name = nameString;
        nameTxData.sparkAddress = getAddress(incomingViewKey, diversifier).encode(addressNetwork(isTestNet));
        nameTxData.sparkNameValidityBlocks = static_cast<uint32_t>(sparkNameValidityBlocks);
        nameTxData.additionalInfo = infoString;
        nameTxData.hashFailsafe = hashFailSafe;

        std::vector<unsigned char> outputScript;
        if (withoutProof) {
            outputScript.clear();
            nameTxData.addressOwnershipProof.clear();
            CDataStream sparkNameDataStream(SER_NETWORK, PROTOCOL_VERSION);
            sparkNameDataStream << nameTxData;
            outputScript.insert(outputScript.end(), sparkNameDataStream.begin(), sparkNameDataStream.end());
        } else {
            std::string mHex(scalarMHex);
            Scalar m;
            try {
                m.SetHex(mHex);
            } catch (...) {
                SparkNameScript* result = abiAllocArray<SparkNameScript>(1);
                if (result == nullptr) {
                    return nullptr;
                }
                result->error = abiStrdup("hash fail");

                return result;
            }
            GetSparkNameScript(nameTxData, m, spendKey, incomingViewKey, outputScript);
        }

        std::size_t sparkNameTxDataSize = getSparkNameTxDataSize(nameTxData);

        SparkNameScript* result = abiAllocArray<SparkNameScript>(1);
        if (result == nullptr) {
            return nullptr;
        }

        result->script = static_cast<unsigned char*>(abiAlloc(outputScript.size(), 1));
        if (result->script == nullptr) {
            free(result);
            return nullptr;
        }
        result->scriptLength = static_cast<int>(outputScript.size());
        result->size = static_cast<int>(sparkNameTxDataSize);

        memcpy(result->script, outputScript.data(), outputScript.size());

        return result;
    } catch (const std::exception& e) {
        SparkNameScript* result = abiAllocArray<SparkNameScript>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->error = abiStrdup(e.what());

        return result;
    } catch (...) {
        SparkNameScript* result = abiAllocArray<SparkNameScript>(1);
        if (result == nullptr) {
            return nullptr;
        }
        result->error = abiStrdup(kUnknownError);

        return result;
    }
}

SPARK_EXPORT
void native_free(void* ptr) {
    free(ptr);
}

#ifdef __cplusplus
}
#endif
