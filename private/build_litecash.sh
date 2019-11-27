
rm CMakeCache.*

make clean

cmake -DCMAKE_BUILD_TYPE=Release -DLITECASH_LINK_TYPE=Static -DLITECASH_NO_QT_UI_WALLET=On .
