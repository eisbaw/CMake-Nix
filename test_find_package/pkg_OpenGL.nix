# Nix package mapping for OpenGL
# TODO: This file provides the OpenGL packages from nixpkgs
# TODO: Adjust if you need specific GL implementations (Mesa, proprietary drivers, etc.)
{ pkgs ? import <nixpkgs> {} }:

{
  inherit (pkgs) libGL libGLU;
}