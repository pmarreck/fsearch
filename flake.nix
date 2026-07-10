{
  description = "FSearch with reproducible Meson build, tests, and development shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      perSystem = system:
        let
          pkgs = import nixpkgs { inherit system; };
          fsearch = pkgs.stdenv.mkDerivation {
            pname = "fsearch";
            version = "0.3-beta2";
            src = self;

            nativeBuildInputs = with pkgs; [
              appstream
              gettext
              itstool
              libxml2
              meson
              ninja
              pkg-config
            ];

            buildInputs = with pkgs; [
              glib
              gtk3
              icu
              pcre2
            ];

            mesonFlags = [ "--buildtype=release" ];
            mesonBuildDir = "builddir";
            doCheck = true;
          };
        in
        {
          packages.default = fsearch;

          apps.default = {
            type = "app";
            program = "${fsearch}/bin/fsearch";
            meta.description = "FSearch desktop and command-line file search utility";
          };

          checks = {
            build = fsearch;
            test = pkgs.runCommand "fsearch-test" {
              nativeBuildInputs = [ pkgs.bash ];
            } ''
              bash ${self}/tests/cli/test_cli ${fsearch}/bin/fsearch
              touch $out
            '';
          };

          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              appstream
              cppcheck
              gettext
              hyperfine
              itstool
              jq
              libxml2
              meson
              ninja
              pkg-config
            ];

            buildInputs = with pkgs; [
              glib
              gtk3
              icu
              pcre2
            ];
          };
        };
      forAllSystems = f: nixpkgs.lib.genAttrs systems f;
    in
    {
      packages = forAllSystems (system: (perSystem system).packages);
      apps = forAllSystems (system: (perSystem system).apps);
      checks = forAllSystems (system: (perSystem system).checks);
      devShells = forAllSystems (system: (perSystem system).devShells);
    };
}
