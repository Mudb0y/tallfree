{
  description = "Tallfree: a screen reader for the Roland SP-404MKII";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in
    {
      # nix develop: the engine, the image and the recordings. Nix's python3
      # does not see a capstone merely installed beside it, hence withPackages.
      devShells.x86_64-linux.default = pkgs.mkShellNoCC {
        packages = [
          pkgs.gcc-arm-embedded
          pkgs.gnumake
          pkgs.qemu
          (pkgs.python3.withPackages (ps: [ ps.capstone ps.numpy ]))
        ];
      };
      # Ghidra is not in the shell, which would pull a JDK into every engine
      # build. ghidra/gdec and ghidra/import.sh take it from the nixpkgs
      # revision in flake.lock, so the project only meets the Ghidra it names.
    };
}
