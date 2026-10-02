#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../electrum_libsparkmobile.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); failures++; } \
} while (0)

static unsigned char key[32];

struct OwnCoin {
    struct CCDataStream coin;
    struct CCDataStream context;
};

static int makeOwnCoin(const char* address, uint64_t value, int i, struct OwnCoin* out) {
    unsigned char txHash[32];
    memset(txHash, 0, sizeof(txHash));
    memcpy(txHash, &i, sizeof(i));
    struct TxInputData input = {txHash, 32, i};
    struct SerializedMintContextResult* ctx = serializeMintContext(&input, 1);
    if (!ctx) return 0;

    struct CMintedCoinData output = {address, value, ""};
    struct CCRecipientList* recipients = cCreateSparkMintRecipients(
            &output, 1, ctx->context, ctx->contextLength, 1, 0);
    if (!recipients || recipients->length != 1) return 0;
    out->coin.length = recipients->list[0].pubKeyLength - 1;
    out->coin.data = malloc(out->coin.length);
    memcpy(out->coin.data, recipients->list[0].pubKey + 1, out->coin.length);
    out->context.data = ctx->context;
    out->context.length = ctx->contextLength;
    return 1;
}

static void testCandidatesAboveSelectedInputLimit(const char* address) {
    enum { N = 101 };
    static struct OwnCoin own[N];
    static struct SpendCoinData coins[N];
    for (int i = 0; i < N; i++) {
        if (!makeOwnCoin(address, 100000000, i, &own[i])) {
            CHECK(0, "could not create test coin");
            return;
        }
        coins[i].serializedCoin = &own[i].coin;
        coins[i].serializedCoinContext = &own[i].context;
        coins[i].groupId = 1;
        coins[i].height = 100 + i;
    }
    struct SparkFeeResult* many = estimateSparkFee(key, 32, 1, 1000000, 0, coins, N, 1, 0, 0);
    struct SparkFeeResult* one = estimateSparkFee(key, 32, 1, 1000000, 0, coins, 1, 1, 0, 0);
    CHECK(many && !many->error, "101 candidates must be accepted (one is selected)");
    CHECK(one && !one->error, "single candidate estimate failed");
    if (many && one && !many->error && !one->error) {
        CHECK(many->fee > 0 && many->fee == one->fee,
              "selecting one of 101 candidates must cost the same as one");
    }
}

static void testCoinLengthPrefixes(const char* address) {
    void* viewKey = getFullViewKeyFromPrivateKeyData(key, 32, 1);
    CHECK(viewKey != NULL, "full view key");

    unsigned char coin[1 + 3 * 34 + 5];
    memset(coin, 0, sizeof(coin));
    for (int i = 0; i < 3; i++) coin[1 + i * 34] = 0x02;
    unsigned char* size = coin + 1 + 3 * 34;
    size[0] = 0xfe; size[1] = 0x40; size[2] = 0x4b; size[3] = 0x4c; size[4] = 0x00;
    unsigned char context[32] = {0};
    CHECK(idAndRecoverCoinByFullViewKey(coin, sizeof(coin), viewKey, context, 32, 0) == NULL,
          "a coin declaring more data than it carries must be rejected");

    struct OwnCoin own;
    CHECK(makeOwnCoin(address, 12345, 7, &own), "create coin");
    struct AggregateCoinData* recovered = idAndRecoverCoinByFullViewKey(
            own.coin.data, own.coin.length, viewKey, own.context.data, own.context.length, 0);
    CHECK(recovered && recovered->value == 12345, "a valid coin must still recover");
    deleteFullViewKey(viewKey);
}

static void testNegativeAmountRejected(void) {
    struct SparkFeeResult* r = estimateSparkFee(key, 32, 1, -1, 0, NULL, 0, 1, 0, 0);
    CHECK(r && r->error, "negative send amount must be rejected");
}

static void testKeyIndexAndDiversifierMustNotBeNegative(void) {
    CHECK(getAddress(key, 32, -1, 1, 0) == NULL, "negative key index must be rejected");
    CHECK(getAddress(key, 32, 1, -1, 0) == NULL, "negative diversifier must be rejected");
    CHECK(getFullViewKeyFromPrivateKeyData(key, 32, -1) == NULL, "negative index view key");
}

static void testEstimatorOutputLimitMatchesBuilder(void) {
    struct SparkFeeResult* r = estimateSparkFee(key, 32, 1, 1000, 0, NULL, 0, 15, 0, 0);
    CHECK(r && r->error, "15 private outputs must be rejected like the builder does");
}

int main(void) {
    for (int i = 0; i < 32; i++) key[i] = (unsigned char)i;
    const char* address = getAddress(key, 32, 1, 1, 0);
    if (!address) {
        fprintf(stderr, "FAIL: getAddress\n");
        return 1;
    }
    testCandidatesAboveSelectedInputLimit(address);
    testCoinLengthPrefixes(address);
    testNegativeAmountRejected();
    testKeyIndexAndDiversifierMustNotBeNegative();
    testEstimatorOutputLimitMatchesBuilder();
    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("all ABI tests passed\n");
    return 0;
}
