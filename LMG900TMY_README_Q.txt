1. Android build
  - Download original android source code ( android-10.0.0_r39 ) from http://source.android.com   
  ( $repo init -u https://android.googlesource.com/platform/manifest -b android-10.0.0_r39
    $repo sync -cdq -j12 --no-tags
    $repo start android-10.0.0_r39 --all 
  )
  - And, merge the source into the android source code
  
  - Run following scripts to build android
    a) source build/envsetup.sh
    b) lunch 1
    c) make -j4
  - When you compile the android source code, you have to add google original prebuilt source(toolchain) into the android directory.
  - After build, you can find output at out/target/product/generic

2. Kernel Build  
  - When you compile the kernel source code, you have to add google original "prebuilt" source(toolchain) into the android directory.
  
  - Run following scripts to build kernel
    a) cd kernel-4.14
    b) mkdir -p out
    c) ../prebuilts/build-tools/linux-x86/bin/make -j4 -C ./ O=./out  ARCH=arm64 CROSS_COMPILE=../../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android- CLANG_TRIPLE=aarch64-linux-gnu- CC=../../prebuilts/clang/host/linux-x86/clang-r353983c/bin/clang MTK_DTBO_FEATURE=yes lge/cayman_lao-perf_defconfig headers_install
    d) ../prebuilts/build-tools/linux-x86/bin/make -j4 -C ./ O=./out  ARCH=arm64 CROSS_COMPILE=../../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android- CLANG_TRIPLE=aarch64-linux-gnu- CC=../../prebuilts/clang/host/linux-x86/clang-r353983c/bin/clang MTK_DTBO_FEATURE=yes

* "-j4" : The number, 4, is the number of multiple jobs to be invoked simultaneously. 
- After build, you can find the build image(Image.gz) at kernel-4.14/out/arch/arm64/boot

3. to obtain the open data about KnowMeABBA*.apk 

 If you want to obtain the open data ( source ) for "KnowMeABBA*.apk" application, 
 the open data may be obtained at http://opensource.lge.com. 
 LGE will also provide open data ( code ) to you . 
 
 You can search the keyword with "KnowMeABBA" at the search window in http://opensource.lge.com.  

 You can find it in "Software List" at that website.


