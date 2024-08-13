// SPDX-License-Identifier: GPL-3.0-or-later

fn main() {
    let arch = std::env::var("CARGO_CFG_TARGET_ARCH")
        .unwrap()
        .to_lowercase();

    let project_root_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let project_root_dir = std::path::Path::new(&project_root_dir)
        .parent() // drivers
        .unwrap()
        .parent() // userspace
        .unwrap()
        .parent() // MOS
        .unwrap();

    macro_rules! build_dir {
        () => {
            format!("{}/build/{}/", project_root_dir.display(), arch);
        };
        ($dir:expr) => {
            format!("{}/build/{}/{}", project_root_dir.display(), arch, $dir)
        };
    }

    macro_rules! project_dir {
        () => {
            format!("{}/", project_root_dir.display());
        };
        ($dir:expr) => {
            format!("{}/{}", project_root_dir.display(), $dir)
        };
    }

    println!(
        "cargo:rustc-link-search=native={}",
        build_dir!("/userspace/drivers/libdma")
    );

    println!("cargo:rustc-link-lib=dma");
    println!(
        "cargo:rerun-if-changed={}",
        project_dir!("proto/blockdev.proto")
    );

    protobuf_codegen::Codegen::new()
        .include(project_dir!("proto"))
        .include(build_dir!("nanopb_workdir/proto"))
        .input(project_dir!("proto/blockdev.proto"))
        .input(build_dir!("nanopb_workdir/proto/nanopb.proto"))
        .input(project_dir!("proto/mosrpc.proto"))
        .cargo_out_dir("protos")
        .run_from_script();
}
