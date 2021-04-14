#!/bin/bash

version=$1

if [ -z "$version" ] ; then
	echo "Error: not enough arguments" 1>&2
	echo "Usage: $0 version" 1>&2
	exit 1
fi

rootdir=$(git rev-parse --show-toplevel)

cd "$rootdir"

git submodule update --init --recursive

update_submodules() {
	local reporoot=$1
	local relativedir=$2
	local url=''
	local hash=''
	local submodule=''
	local path=''
	local new_relativedir=''

	while read submodule ; do
		path=$(git config --file .gitmodules submodule.${submodule}.path)
		hash=$(git submodule status ${path} | awk '{ print $1}' | sed -e 's:^\+::')
		url=$(git config --file .gitmodules submodule.${submodule}.url)
		branch=$(git config --file .gitmodules submodule.${submodule}.branch)

		if [ -z "$relativedir" ] ; then
			new_relativedir="$path"
		else
			new_relativedir="$relativedir/$path"
		fi

		pushd "${reporoot}" 1>&2
		git fetch "${url}" "${hash}" 1>&2
		fetchresult=$?

		if [ ${fetchresult} -ne 0 ] && [ -n "${branch}" ] ; then
			git fetch "${url}" "${branch}" 1>&2
			fetchresult=$?
		fi

		if [ ${fetchresult} -ne 0 ] ; then
			git fetch "${url}" 1>&2
		fi

		git tag "${new_relativedir}-${version}" "${hash}" 1>&2
		popd 1>&2

		pushd "${path}" 1>&2
		update_submodules "${reporoot}" "${new_relativedir}"
		popd 1>&2

		echo "${new_relativedir}-${version}"
	done < <(git config --file .gitmodules --name-only --get-regexp url | sed -e 's:^submodule\.::' -e 's:\.url$::')
}

tags=$(update_submodules $rootdir '')
tags="$(echo "${tags}" | sort)"
git merge --no-edit -s ours --allow-unrelated-histories $(echo "${tags}" | xargs echo)

echo "Following tags should be merged:"
echo "${tags}"
echo
echo "Following gear rules might be needed:"
while read tag ; do
	echo "tar: $(echo $tag | sed -e "s:$version:@version@:g"):. name=@name@-@version@-$(echo $tag | sed -e "s:-$version::g" -e "s:/:-:g") base=$(echo $tag | sed -e "s:-$version::g")"
done < <(echo "${tags}")
