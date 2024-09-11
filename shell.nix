with import <nixpkgs> { };
let
    inherit (llvmPackages_18) stdenv libcxx;
in
mkShell.override { inherit stdenv; } {
  buildInputs = [
    libcxx
    libxml2
  ];
  nativeBuildInputs = [
    clang_18
    cmake
    lld_18
    llvm_18
    ninja
  ];

  shellHook = ''
    export CXXFLAGS="-isystem ${libcxx.dev}/include/c++/v1"
    export LDFLAGS="-fuse-ld=lld"
  '';
}
