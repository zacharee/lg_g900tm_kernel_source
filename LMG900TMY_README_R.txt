1. Android build
  - Download original android source code ( android-11.0.0_r35 ) from http://source.android.com   
  ( $repo init -u https://android.googlesource.com/platform/manifest -b android-11.0.0_r35
    $repo sync -cdq -j12 --no-tags
    $repo start android-11.0.0_r35 --all 
  )
  - And, merge the source into the android source code
  - Run following scripts to build android
    a) source build/envsetup.sh
    b) lunch 1
    c) make -j4
  - When you compile the android source code, you have to add google original prebuilt source(toolchain) into the android directory.
  - After build, you can find output at out/target/product/generic

2. Kernel Build  

  - Uncompress using following command at the android directory

    a) tar -xvzf *_Kernel_R.tar.gz

  - When you compile the kernel source code, you have to add google original "prebuilt" source(toolchain) into the android directory.
  - Run following scripts to build kernel
    a) cd kernel-4.14
    b) mkdir -p out
    c) make -C ./ O=./out ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/lib64:$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/bin:../../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:/usr/bin:/bin:$PATH LGE_TARGET_PLATFORM=mt6885 LGE_TARGET_DEVICE=mcaymanlm LGE_LAMP_DEVICE=yes muse6883_caymanlm_r_defconfig
    d) make -C ./ O=./out ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/lib64:$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/bin:../../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:/usr/bin:/bin:$PATH LGE_TARGET_PLATFORM=mt6885 LGE_TARGET_DEVICE=mcaymanlm LGE_LAMP_DEVICE=yes headers_install
    e) make -C ./ O=./out ARCH=arm64 CROSS_COMPILE=aarch64-linux-androidkernel- CLANG_TRIPLE=aarch64-linux-gnu- LD=ld.lld LD_LIBRARY_PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/lib64:$LD_LIBRARY_PATH NM=llvm-nm OBJCOPY=llvm-objcopy CC=clang PATH=../../prebuilts/clang/host/linux-x86/clang-r383902/bin:../../prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin:/usr/bin:/bin:$PATH LGE_TARGET_PLATFORM=mt6885 LGE_TARGET_DEVICE=mcaymanlm LGE_LAMP_DEVICE=yes -j4


* "-j4" : The number, 4, is the number of multiple jobs to be invoked simultaneously. 
- After build, you can find the build image(Image.gz) at out/arch/arm64/boot/ 

3. to obtain the open data about KnowMeABBA*.apk App  

If you want to obtain the open data ( source ) for "KnowMeABBA or *ABBA.apk " application, 
 the open data may be obtained at http://opensource.lge.com. 
 LGE will also provide open data ( code ) to you . 
 
 You can search the keyword with "KnowMeABBA" at the search window in http://opensource.lge.com.  

You can find it in "Software List" at that website.