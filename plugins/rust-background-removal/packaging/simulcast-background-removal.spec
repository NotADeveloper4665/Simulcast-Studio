%global debug_package %{nil}
Name:           simulcast-background-removal
Version:        0.1.0
Release:        1%{?dist}
Summary:        Native ONNX background removal filter for Simulcast Studio
License:        GPL-2.0-or-later
URL:            https://github.com/NotADeveloper4665/Simulcast-Studio
Source0:        libsimulcast_background_removal.so
Source1:        README.md
Source2:        gplv2.txt
Requires:       onnxruntime >= 1.22

%description
Native Rust effect filter for the branded Simulcast libobs installation.
Uses an externally supplied U2Netp ONNX model.

%prep
%build
%install
install -Dm755 %{SOURCE0} %{buildroot}%{_libdir}/obs-studio-rust/obs-modules/core/simulcast-background-removal.so
install -Dm644 %{SOURCE1} %{buildroot}%{_docdir}/%{name}/README.md
install -Dm644 %{SOURCE2} %{buildroot}%{_licensedir}/%{name}/gplv2.txt

%files
%{_libdir}/obs-studio-rust/obs-modules/core/simulcast-background-removal.so
%doc %{_docdir}/%{name}/README.md
%license %{_licensedir}/%{name}/gplv2.txt
