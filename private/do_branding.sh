#!/bin/sh


find . -type f -name "*" -exec sed -i'' -e 's/BEAM_SUFFIX_LOWER_CASE/LITECASH_SUFFIX_LOWER_CASE/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_SUFFIX_UPPER_CASE/LITECASH_SUFFIX_UPPER_CASE/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_TARGET_SUFFIX/LITECASH_TARGET_SUFFIX/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_SIGN_PACKAGE/LITECASH_SIGN_PACKAGE/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_SIGN_SUBJECT_NAME/LITECASH_SIGN_SUBJECT_NAME/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_VERSION/LITECASH_VERSION/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_USE_AVX/LITECASH_USE_AVX/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_CPU_ARCH/LITECASH_CPU_ARCH/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_QT_UI_WALLET/LITECASH_QT_UI_WALLET/g' {} +

find . -type f -name "*" -exec sed -i'' -e 's/BEAM_USE_STATIC/LITECASH_USE_STATIC/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_MAINNET/LITECASH_MAINNET/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_TESTNET/LITECASH_TESTNET/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_DISPLAY_SUFFIX/LITECASH_DISPLAY_SUFFIX/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_SIGN_MACOS_BINARY/LITECASH_SIGN_MACOS_BINARY/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_INSTALLER_NAME/LITECASH_INSTALLER_NAME/g' {} +

find . -type f -name "*" -exec sed -i'' -e 's/BEAM_BUILD_JNI/LITECASH_BUILD_JNI/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_DESKTOP_DIR/LITECASH_DESKTOP_DIR/g' {} +
find . -type f -name "*" -exec sed -i'' -e 's/BEAM_PIXMAPS_DIR/LITECASH_PIXMAPS_DIR/g' {} +


