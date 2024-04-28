# 
# This is a nix flake I use to create a dev environment for lpp.
# This is not meant to be the standard way to build lpp, its just a way 
# for me to quickly get a consistent env on whatever system im working on,
# when that system is x86_64 linux.
#
{
	description = "Flake providing a development environment for lpp.";
	inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
	inputs.flake-utils.url = "github:numtide/flake-utils";

	outputs = { nixpkgs, flake-utils, ... }:
		flake-utils.lib.eachDefaultSystem (system:
			let
				pkgs = nixpkgs.legacyPackages.${system};
			in
			{
				devShells.default = pkgs.mkShell {
				packages = with pkgs;
					[ 
						luajit
						(hiPrio clang-tools.override 
						{
							llvmPackages = llvmPackages_18;
							enableLibcxx = false;
						})
						llvmPackages_18.libcxxClang
						llvmPackages_18.libclang
						llvmPackages_18.libllvm
						llvmPackages_18.stdenv
						gnumake
						gdb
						bear
						linuxKernel.packages.linux_zen.perf
					];

				env = 
				{
					LD_LIBRARY_PATH = "${pkgs.llvmPackages_18.stdenv.cc.cc.lib}/lib";
				};
			};
		});
}
