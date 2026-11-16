#!/usr/bin/env bash
# Host-only test run (no PS3 SDK needed). Builds and runs the protocol, audio,
# discovery, metadata and UI tests with ASan/UBSan.
#
#   MBEDTLS_INCLUDE=/path/to/mbedtls/include \
#   MBEDTLS_HOST_LIBRARY=/path/to/host/libmbedcrypto.a \
#   scripts/run-host-tests.sh
set -euo pipefail
cd "$(dirname "$0")/.."
: "${CC:=cc}"
: "${MBEDTLS_INCLUDE:?Set MBEDTLS_INCLUDE to Mbed TLS 3.6.x headers}"
: "${MBEDTLS_HOST_LIBRARY:?Set MBEDTLS_HOST_LIBRARY to a host libmbedcrypto.a}"
OUT=build/host-tests
mkdir -p "$OUT"
SAN=(-g -fsanitize=address,undefined)
export UBSAN_OPTIONS=halt_on_error=1 ASAN_OPTIONS=halt_on_error=1
SESSION_SOURCES=(src/crypto/raop_crypto.c src/protocol/raop_sdp.c
	src/protocol/raop_session.c src/protocol/raop_metadata.c)
CRYPTO_FLAGS=(-Isrc/crypto -Isrc/protocol -I"$MBEDTLS_INCLUDE" -DMBEDTLS_NO_PLATFORM_ENTROPY)

run() { echo "== $1"; "$OUT/$1"; }

"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/crypto -I"$MBEDTLS_INCLUDE" \
	-DMBEDTLS_NO_PLATFORM_ENTROPY src/crypto/raop_crypto.c \
	tests/protocol/raop_crypto_test.c "$MBEDTLS_HOST_LIBRARY" -o "$OUT/raop_crypto_test"
run raop_crypto_test

for test in raop_session_crypto_test raop_session_test raop_metadata_test; do
	"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" "${CRYPTO_FLAGS[@]}" \
		"${SESSION_SOURCES[@]}" "tests/protocol/$test.c" "$MBEDTLS_HOST_LIBRARY" -lm \
		-o "$OUT/$test"
	run "$test"
done

"$CC" -std=c11 -Wall -Wextra -Werror "${CRYPTO_FLAGS[@]}" "${SESSION_SOURCES[@]}" \
	tests/protocol/raop_sdp_test.c "$MBEDTLS_HOST_LIBRARY" -lm -o "$OUT/raop_sdp_test"
run raop_sdp_test

"$CC" -D_DEFAULT_SOURCE -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/discovery \
	src/discovery/raop_mdns.c src/discovery/raop_mdns_service.c \
	tests/discovery/raop_mdns_test.c -o "$OUT/raop_mdns_test"
run raop_mdns_test

"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -pthread "${CRYPTO_FLAGS[@]}" \
	-Isrc/observability -DRAOP_VERBOSE_LOGGING=1 "${SESSION_SOURCES[@]}" \
	src/protocol/raop_rtp.c src/protocol/raop_receiver.c src/observability/receiver_log.c \
	tests/protocol/raop_receiver_test.c "$MBEDTLS_HOST_LIBRARY" -lm -o "$OUT/raop_receiver_test"
run raop_receiver_test

"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -pthread "${CRYPTO_FLAGS[@]}" \
	-Isrc/codec -Isrc/audio -Isrc/observability "${SESSION_SOURCES[@]}" \
	src/protocol/raop_rtp.c src/protocol/raop_receiver.c src/codec/alac.c \
	src/codec/raop_alac.c src/audio/pcm_ring.c src/observability/receiver_log.c \
	tests/protocol/raop_audio_pipeline_test.c "$MBEDTLS_HOST_LIBRARY" -lm \
	-o "$OUT/raop_audio_pipeline_test"
run raop_audio_pipeline_test

"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/codec -Isrc/protocol \
	src/codec/alac.c src/codec/raop_alac.c tests/protocol/raop_alac_test.c \
	-o "$OUT/raop_alac_test"
run raop_alac_test

for test in pcm_ring_test pcm_quality_test; do
	"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/audio src/audio/pcm_ring.c \
		"tests/audio/$test.c" -lm -o "$OUT/$test"
	run "$test"
done
"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/audio src/audio/pcm_gain.c \
	tests/audio/pcm_gain_test.c -o "$OUT/pcm_gain_test"
run pcm_gain_test

"$CC" -std=c11 -Wall -Wextra -Werror "${SAN[@]}" -Isrc/ui src/ui/*.c \
	tests/ui/ui_test.c -lm -o "$OUT/ui_test"
run ui_test

"$CC" -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror "${SAN[@]}" -pthread \
	-DFLIP_TIMEOUT_US=100000U -DSETTINGS_SAVE_DELAY_US=200000U \
	-Itests/observability/mocks -Iprototype/standalone-receiver/source -Isrc/ui \
	prototype/standalone-receiver/source/visual_renderer.c src/ui/*.c \
	tests/observability/visual_renderer_test.c -lm -o "$OUT/visual_renderer_test"
mkdir -p build/forensic
run visual_renderer_test

echo "All host tests passed."
