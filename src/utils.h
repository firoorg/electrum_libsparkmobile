#ifndef ORG_FIRO_SPARK_UTILS_H
#define ORG_FIRO_SPARK_UTILS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "structs.h"
#include "deps/sparkmobile/include/spark.h"

spark::SpendKey createSpendKeyFromData(unsigned char *keyData, int index);

spark::Coin coinFromCCDataStream(CCDataStream& cdStream);

CScript createCScriptFromBytes(const unsigned char* bytes, int length);

std::vector<unsigned char> serializeCScript(const CScript& script);

bool isValidAddressPoint(const secp_primitives::GroupElement& point);

void requireValidAddressPoints(const spark::Address& address);

spark::Address decodeAddress(const std::string& str);
spark::Address decodeAddress(const std::string& str, unsigned char expectedNetwork);

const int kMaxSerializedCoinSize = 64 * 1024;

spark::Coin deserializeCoin(const unsigned char *serializedCoin, int length);

#endif
