//! One module per workspace tab, dispatched by `super::render` and
//! `super::run_app` based on the active [`super::Tab`].

pub mod packages;
pub mod system;
pub mod tsi;
