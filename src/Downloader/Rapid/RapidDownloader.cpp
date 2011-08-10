#include "RapidDownloader.h"
#include "../../FileSystem.h"
#include "../../Util.h"
#include "Sdp.h"
#include <stdio.h>
#include <string>
#include <string.h>
#include <list>
#include "RepoMaster.h"


CRapidDownloader::CRapidDownloader(){
	std::string url(REPO_MASTER);
	this->repoMaster=new CRepoMaster(url, this);
	reposLoaded = false;
	sdps.clear();
}

CRapidDownloader::~CRapidDownloader(){
	sdps.clear();
	delete(repoMaster);
}


void CRapidDownloader::addRemoteDsp(CSdp& sdp){
	sdps.push_back(sdp);
}

/**
	helper function for sort
*/
static bool list_compare(CSdp& first ,CSdp& second){
	std::string name1;
	std::string name2;
	name1.clear();
	name2.clear();
	name1=(first.getShortName());
	name2=(second.getShortName());
	unsigned int len;
	len=std::min(name1.size(), name2.size());
	for (unsigned int i=0;i<len;i++){
		if (tolower(name1[i])<tolower(name2[i])){
			return true;
		}
	}
	return false;
}
/**
	update all repos from the web
*/
bool CRapidDownloader::reloadRepos(){
	if (reposLoaded)
		return true;
	std::string url(REPO_MASTER);
	this->repoMaster->download(url);
	repoMaster->updateRepos();
	reposLoaded=true;
	return true;
}

/**
	download by name, for example "Complete Annihilation revision 1234"
*/
bool CRapidDownloader::download_name(const std::string& longname, int reccounter){
	DEBUG_LINE("%s",longname.c_str());
	std::list<CSdp>::iterator it;
	if (reccounter>10)
		return false;
	for (it=sdps.begin();it!=sdps.end();++it){
		if (match_download_name((*it).getName(),longname)){
			printf("Found Depends, downloading %s\n", (*it).getName().c_str());
			if (!(*it).download())
				return false;
			if ((*it).getDepends().length()>0){
				if (!download_name((*it).getDepends(),reccounter+1))
					return false;
			}
			return true;
		}
	}
	return false;
}


/**
	search for a mod, searches for the short + long name
*/
bool CRapidDownloader::search(std::list<IDownload>& result, const std::string& name, IDownload::category cat){
	DEBUG_LINE("%s",name.c_str());
	reloadRepos();

	sdps.sort(list_compare);
	std::list<CSdp>::iterator it;
	for (it=sdps.begin();it!=sdps.end();++it){
		if (match_download_name((*it).getShortName().c_str(),name)
				|| (match_download_name((*it).getName().c_str(),name))){
			IDownload dl=IDownload((*it).getShortName().c_str(),(*it).getName().c_str());
			result.push_back(dl);
		}
	}
	return true;
}
/**
	start a download
*/
bool CRapidDownloader::download(IDownload& download){
	DEBUG_LINE("%s",download.name.c_str());
	reloadRepos();
	return download_name(download.name,0);
}
