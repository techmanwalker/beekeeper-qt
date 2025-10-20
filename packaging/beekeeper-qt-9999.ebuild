# File: app-admin/beekeeper-qt/beekeeper-qt-9999.ebuild
EAPI=8

# Base eclasses are inherited unconditionally
inherit cmake qt6-build

DESCRIPTION="Qt GUI tool for Btrfs deduplication and compression via the bees daemon"
HOMEPAGE="https://github.com/techmanwalker/beekeeper-qt"
LICENSE="GPL-3"

if [[ ${PV} == *9999* ]]; then
    # For live ebuild, inherit git-r3 and set the Git repository URI.
    inherit git-r3
    EGIT_REPO_URI="https://github.com/techmanwalker/beekeeper-qt.git"
    
    SRC_URI="" 
else
    # For stable releases (e.g., if symlinked to v1.0.0), use the tarball SRC_URI.
    # Note: Using the tag format 'v${PV}' is common for GitHub archives.
    SRC_URI="https://github.com/techmanwalker/beekeeper-qt/archive/refs/tags/v${PV}.tar.gz -> ${P}.tar.gz"
fi

S="${WORKDIR}/${PN}"

IUSE=""

RDEPEND="
    sys-fs/bees
    dev-qt/qtbase:6[widgets,concurrent,dbus]
"

DEPEND="${RDEPEND}
    dev-util/cmake
    dev-qt/qtbase:6[widgets,concurrent,dbus] 
	dev-build/ninja
	sys-auth/polkit-qt
	virtual/pkgconfig
	sys-apps/util-linux
	# Provides necessary Qt tools like Qt6LinguistTools