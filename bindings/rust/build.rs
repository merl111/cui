use std::{env, path::PathBuf, process::Command};
fn main() {
    println!("cargo:rerun-if-env-changed=CUI_LIB_DIR");
    println!("cargo:rerun-if-env-changed=PKG_CONFIG");
    let dir = env::var_os("CUI_LIB_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|| {
            PathBuf::from(env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("../../build")
        });
    println!("cargo:rustc-link-search=native={}", dir.display());
    println!("cargo:rustc-link-lib=static=cui");
    let target = env::var("CARGO_CFG_TARGET_OS").unwrap();
    println!(
        "cargo:rerun-if-changed={}",
        dir.join(
            if target == "windows" && env::var("CARGO_CFG_TARGET_ENV").as_deref() == Ok("msvc") {
                "cui.lib"
            } else {
                "libcui.a"
            }
        )
        .display()
    );
    match target.as_str() {
        "linux" => {
            println!("cargo:rustc-link-lib=m");
            let output =
                Command::new(env::var_os("PKG_CONFIG").unwrap_or_else(|| "pkg-config".into()))
                    .args(["--libs", "gtk4-x11", "xext"])
                    .output()
                    .expect("Install pkg-config and GTK 4 development files");
            assert!(
                output.status.success(),
                "pkg-config gtk4 failed: {}",
                String::from_utf8_lossy(&output.stderr)
            );
            for flag in String::from_utf8(output.stdout).unwrap().split_whitespace() {
                if let Some(lib) = flag.strip_prefix("-l") {
                    println!("cargo:rustc-link-lib={lib}");
                } else if let Some(path) = flag.strip_prefix("-L") {
                    println!("cargo:rustc-link-search=native={path}");
                } else {
                    println!("cargo:rustc-link-arg={flag}");
                }
            }
        }
        "macos" => {
            println!("cargo:rustc-link-lib=framework=AppKit");
            println!("cargo:rustc-link-lib=framework=QuartzCore");
        }
        "windows" => {
            for lib in [
                "comctl32", "gdiplus", "user32", "gdi32", "dwmapi", "advapi32", "ole32", "oleacc",
                "shell32", "uuid", "comdlg32",
            ] {
                println!("cargo:rustc-link-lib={lib}");
            }
        }
        _ => panic!("CUI supports Linux, Windows and macOS"),
    }
}
