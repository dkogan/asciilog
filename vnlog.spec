Name:           vnlog
Version:        xxx
Release:        1%{?dist}
Summary:        Tools to manipulate whitespace-separated ASCII logs

License:        LGPL-2.1+
URL:            https://github.com/dkogan/vnlog/
Source0:        https://github.com/dkogan/vnlog/archive/%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires: python2-devel
BuildRequires: python36-devel
BuildRequires: numpy
BuildRequires: python36-numpy
BuildRequires: perl-IPC-Run
BuildRequires: perl-Text-Diff
BuildRequires: perl-String-ShellQuote
BuildRequires: perl-List-MoreUtils
BuildRequires: mawk
BuildRequires: make
BuildRequires: chrpath
BuildRequires: mrbuild >= 0.63

BuildRequires: /usr/bin/pod2man
BuildRequires: perl-autodie
BuildRequires: perl-Data-Dumper


%description
We want to manipulate data logged in a very simple whitespace-separated ASCII
format. The format in compatible with the usual UNIX tools, and thus can be
processed with a multitude of existing methods. Some convenience tools and
library interfaces are provided to create new data in this format and manipulate
existing data

%package devel
Requires:       %{name}%{_isa} = %{version}-%{release}
Summary:        Development files for vnlog

Requires: perl-String-ShellQuote

%description devel
The library needed for the vnlog C interface and the vnl-gen-header
tool needed to define the fields

%package tools
Requires:       %{name}%{_isa} = %{version}-%{release}
Summary:        Tools for manipulating vnlogs
Requires:       mawk
Requires:       perl-Text-Table
Requires:       perl-List-MoreUtils
Requires:       perl-autodie
Requires:       moreutils

%description tools
Various helper tools to make working with vnlogs easier

%prep
%setup -q

%build
make %{?_smp_mflags} all doc

%check
make check

%install
%make_install

mkdir -p %{buildroot}%{_datadir}/zsh/site-functions
cp completions/zsh/* %{buildroot}%{_datadir}/zsh/site-functions

mkdir -p %{buildroot}%{_datadir}/bash-completion/completions
cp completions/bash/* %{buildroot}%{_datadir}/bash-completion/completions

%clean
make clean

%files
%doc
%{_libdir}/*.so.*
%doc %{_mandir}/man3/*
%{_datadir}/perl5
%{python2_sitelib}/*
%{python3_sitelib}/*

%files devel
%{_libdir}/*.so
%{_includedir}/*
%{_bindir}/vnl-gen-header
%doc %{_mandir}/man1/vnl-gen-header.1.gz

%files tools
%{_bindir}/vnl-filter
%{_bindir}/vnl-tail
%{_bindir}/vnl-sort
%{_bindir}/vnl-join
%{_bindir}/vnl-ts
%{_bindir}/vnl-make-matrix
%{_bindir}/vnl-align
%doc %{_mandir}/man1/vnl-filter.1.gz
%doc %{_mandir}/man1/vnl-tail.1.gz
%doc %{_mandir}/man1/vnl-sort.1.gz
%doc %{_mandir}/man1/vnl-join.1.gz
%doc %{_mandir}/man1/vnl-ts.1.gz
%doc %{_mandir}/man1/vnl-make-matrix.1.gz
%doc %{_mandir}/man1/vnl-align.1.gz
%{_datadir}/zsh/*
%{_datadir}/bash-completion/*
