mod adb;
mod cli;
mod game_db;
mod gui;
mod pipeline;
mod signer;
mod smali;

use clap::Parser;

#[derive(Parser)]
#[command(name = "ue-modloader-installer")]
#[command(about = "UE Modloader Installer — Inject modloader into Quest APKs")]
struct Args {
    /// Run in CLI mode instead of GUI
    #[arg(long)]
    cli: bool,

    /// Target game package (e.g. com.Armature.VR4) — CLI only
    #[arg(long)]
    package: Option<String>,

    /// ADB device serial — CLI only
    #[arg(long, short)]
    serial: Option<String>,

    /// Path to prebuilt libmodloader.so — defaults to ./libmodloader.so
    #[arg(long)]
    modloader_so: Option<String>,

    /// Patch a local APK file instead of pulling from device.
    /// No ADB/device needed — just injects the modloader into the APK.
    #[arg(long)]
    apk: Option<String>,

    /// Output path for the patched APK (used with --apk)
    #[arg(long, short)]
    output: Option<String>,

    /// Skip all confirmation prompts (auto-yes)
    #[arg(long, short = 'y')]
    yes: bool,
}

fn main() -> anyhow::Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info"))
        .format_timestamp(None)
        .init();

    let args = Args::parse();

    // Local APK patching mode (no device required)
    if let Some(ref apk_path) = args.apk {
        return pipeline::patch_local_apk(
            apk_path,
            args.package.as_deref(),
            args.modloader_so.as_deref(),
            args.output.as_deref(),
        );
    }

    if args.cli {
        cli::run(args.serial, args.package, args.modloader_so, args.yes)?;
    } else {
        gui::run(args.modloader_so)?;
    }

    Ok(())
}
