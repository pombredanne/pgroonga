#!/bin/sh

run()
{
  "$@"
  if test $? -ne 0; then
    echo "Failed $@"
    exit 1
  fi
}

rpmbuild_options=

. /vagrant/env.sh

distribution=$(cut -d " " -f 1 /etc/redhat-release | tr "A-Z" "a-z")
if grep -q Linux /etc/redhat-release; then
  distribution_version=$(cut -d " " -f 4 /etc/redhat-release)
else
  distribution_version=$(cut -d " " -f 3 /etc/redhat-release)
fi
distribution_version=$(echo ${distribution_version} | sed -e 's/\..*$//g')

architecture="$(arch)"
case "${architecture}" in
  i*86)
    architecture=i386
    ;;
esac

if [ ${PG_PACKAGE_VERSION} -ge 94 ]; then
  pgdg_rpm=pgdg-centos${PG_PACKAGE_VERSION}-${PG_VERSION}-3.noarch.rpm
else
  pgdg_rpm=pgdg-centos${PG_PACKAGE_VERSION}-${PG_VERSION}-2.noarch.rpm
fi
run wget --no-check-certificate https://yum.postgresql.org/${PG_VERSION}/redhat/rhel-${distribution_version}-${architecture}/${pgdg_rpm}
run rpm -ivh ${pgdg_rpm}
groonga_release_rpm=groonga-release-1.3.0-1.noarch.rpm
groonga_release_rpm_url=https://packages.groonga.org/centos/${groonga_release_rpm}
run yum install -y ${groonga_release_rpm_url}

run yum groupinstall -y "Development Tools"
run yum install -y rpm-build rpmdevtools tar ${DEPENDED_PACKAGES}

if [ -x /usr/bin/rpmdev-setuptree ]; then
  rm -rf .rpmmacros
  run rpmdev-setuptree
else
  run cat <<EOM > ~/.rpmmacros
%_topdir ${HOME}/rpmbuild
EOM
  run mkdir -p ~/rpmbuild/SOURCES
  run mkdir -p ~/rpmbuild/SPECS
  run mkdir -p ~/rpmbuild/BUILD
  run mkdir -p ~/rpmbuild/RPMS
  run mkdir -p ~/rpmbuild/SRPMS
fi

repository="/vagrant/repositories/${distribution}/${distribution_version}"
rpm_dir="${repository}/${architecture}/Packages"
srpm_dir="${repository}/source/SRPMS"
run mkdir -p "${rpm_dir}" "${srpm_dir}"

# for debug
# rpmbuild_options="$rpmbuild_options --define 'optflags -O0 -g3'"

cd

if [ -n "${SOURCE_ARCHIVE}" ]; then
  run cp /vagrant/tmp/${SOURCE_ARCHIVE} rpmbuild/SOURCES/
else
  run cp /vagrant/tmp/${PACKAGE}-${VERSION}.* rpmbuild/SOURCES/
fi
run cp /vagrant/tmp/${distribution}/${PACKAGE}.spec rpmbuild/SPECS/

if grep -q -E 'Requires:\s+msgpack' rpmbuild/SPECS/${PACKAGE}.spec; then
  run yum install -y epel-release
  run yum install -y msgpack-devel
fi

run rpmbuild -ba ${rpmbuild_options} rpmbuild/SPECS/${PACKAGE}.spec

run mv rpmbuild/RPMS/*/* "${rpm_dir}/"
run mv rpmbuild/SRPMS/* "${srpm_dir}/"
