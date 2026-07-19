{
  description = "FSearch with reproducible Meson build, tests, and development shell";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "aarch64-darwin" ];
      perSystem = system:
        let
          pkgs = import nixpkgs { inherit system; };
          testEnvironment = ''
            export FSEARCH_TEST_ROOT="$TMPDIR/fsearch-test-state"
            export HOME="$FSEARCH_TEST_ROOT/home"
            export XDG_CONFIG_HOME="$FSEARCH_TEST_ROOT/config"
            export XDG_DATA_HOME="$FSEARCH_TEST_ROOT/data"
            export XDG_CACHE_HOME="$FSEARCH_TEST_ROOT/cache"
            export XDG_STATE_HOME="$FSEARCH_TEST_ROOT/state"
            export XDG_RUNTIME_DIR="$FSEARCH_TEST_ROOT/runtime"
            mkdir -p "$HOME" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME" "$XDG_RUNTIME_DIR"
            chmod 700 "$XDG_RUNTIME_DIR"
          '';
          fsearch = pkgs.stdenv.mkDerivation {
            pname = "fsearch";
            version = "0.3-beta2";
            src = self;

            nativeBuildInputs = with pkgs; [
              appstream
              gettext
              itstool
              jq
              libxml2
              meson
              makeWrapper
              ninja
              pkg-config
              cppcheck
              wrapGAppsHook3
            ];

            buildInputs = with pkgs; [
              glib
              gtk3
              icu
              pcre2
            ];

            mesonFlags = [ "--buildtype=release" ];
            mesonBuildDir = "builddir";
            preFixup = pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
              gappsWrapperArgs+=(
                --set-default LOCALE_ARCHIVE ${pkgs.glibcLocales}/lib/locale/locale-archive
              )
            '';
            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ${testEnvironment}
              bash $src/tests/isolation/test_environment
              meson test --print-errorlogs
              bash $src/tests/static-analysis .. .
              bash $src/tests/i18n/test_translate_po $src/tools/translate-po
              bash $src/tests/i18n/test_catalogs $src
              bash $src/tests/i18n/test_required_locales $src/tools/check-required-locales
              bash $src/tools/check-required-locales $src --strict
              runHook postCheck
            '';
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
              nativeBuildInputs = [
                pkgs.bash
                pkgs.coreutils
                pkgs.findutils
                pkgs.gawk
                pkgs.gettext
                pkgs.gnugrep
                pkgs.jq
                pkgs.ripgrep
              ] ++ pkgs.lib.optionals pkgs.stdenv.hostPlatform.isLinux [ pkgs.glibc.bin ];
            } ''
              ${testEnvironment}
              bash ${self}/tests/isolation/test_environment
              bash ${self}/tests/i18n/test_required_locales ${self}/tools/check-required-locales
              bash ${self}/tools/check-required-locales ${self} --strict
              ${pkgs.lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
                bash ${self}/tests/package/test_gsettings_runtime ${fsearch}/bin/fsearch
              ''}
              bash ${self}/tests/cli/test_cli ${fsearch}/bin/fsearch
              touch $out
            '';
          };

          devShells.default = pkgs.mkShell {
            packages = with pkgs; [
              appstream
              cppcheck
              curl
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
