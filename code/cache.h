#ifndef __CACHE_H__
#define __CACHE__

#include "request.h"
#include "logger.h"

using namespace std;

class Cache
{
public:
	class Response {
	public:
		string exp_time,date_time,Etag;
		vector<char> data;
		bool revalidation;
		size_t size;
		Response(){}
		Response(string exp,string crea,string Etag,int size,vector<char> msg):exp_time(exp),date_time(crea),Etag(Etag),size(size),data(msg){}
	};
	unordered_map<string, Response> respMap;
	string exp_time;
	string date_time;
	string cache_ctrl;
	string Etag;
	string last_modify;
	Logger writer;
	Cache(){
		respMap.clear();
	}

	void initial(vector<char> msg){
		string response;
		for (char c : msg){
			response += c;
		}
		// expire time
		if (response.find("Expires:") != string::npos){
			string time_temp = response.substr(response.find("Expires:") + LEN_EXPIRE);
			exp_time = time_temp.substr(0, time_temp.find("\r\n"));
		}
		// date
		if (response.find("Date:") != string::npos){
			string date_tmp = response.substr(response.find("Date:") + LEN_DATE);
			date_time = date_tmp.substr(0, date_tmp.find("\r\n"));
		}
		// cache control
		if (response.find("Cache-Control:") != string::npos){
			string ctrl_tmp = response.substr(response.find("Cache-Control:") + LEN_CONTENT);
			cache_ctrl = ctrl_tmp.substr(0, ctrl_tmp.find("\r\n"));
		}
		// etag
		if (response.find("ETag:") != string::npos){
			string etag_tmp = response.substr(response.find("ETag:") + LEN_ETAG);
			Etag = etag_tmp.substr(0, etag_tmp.find("\r\n"));
		}
		// last modify time
		if (response.find("Last-Modified:") != string::npos){
			string modify_tmp = response.substr(response.find("Last-Modified:") + LEN_MODIFY);
			last_modify = modify_tmp.substr(0, modify_tmp.find("\r\n"));
		}
	}

	void add_cache(string req, int size, vector<char> msg, int ID)
	{
		string response;
		for (char c : msg) {
			response += c;
		}
		if (no_cache(response,ID)) {
			return;
		}
		// cache_size 512
		if (respMap.size() > 512) {
			respMap.erase(respMap.begin()); // delete the first cache
		}
		Response resp(exp_time,date_time,Etag,size,msg);
		resp.revalidation = (cache_ctrl.find("no-cache") != string::npos) ? true:false;
		resp.revalidation = (cache_ctrl.find("must-revalidate") != string::npos) ? true:false;
		if (resp.revalidation) {
			writer.addMessage(ID, "cached, but requires re-validation");
		}
		writer.addMessage(ID, "cached, expires at ");
		respMap[req] = resp;
	}

	bool find_cache(int client_fd, string req, vector<char> msg, int ID) {
		string response;
		for (char c : msg) {
			response += c;
		}

		if (!hasResponse(req) || needValidation(ID, respMap[req].Etag) || !validInCache(ID, req)) {
			return true;
		}
		else {
			forwardResponse(ID, client_fd, req, response);
			return false;
		}
	}

	// Check if a response is in respMap
	bool hasResponse(string req) {
		return respMap.find(req) != respMap.end();
	}

	// Send response to client
	void forwardResponse(int id, int browser_fd, string req, string response) {
		writer.addMessage(id, "in cache, valid");
		writer.addMessage(id, "Responding " + getHeader(response));
		char * response_data = (respMap[req].data).data();
		size_t resp_size = respMap[req].size;
		if (send(browser_fd, response_data, resp_size, 0) == -1)
		{
			writer.addMessage(id, "Failure sending response to browser.");
		}
	}

	// Get header
	string getHeader(string response) {
		int ind = response.find("\r\n");
		return response.substr(0, ind);
	}

	// Check validation
	bool needValidation(int id, string newTag) {
		if (newTag == Etag) {
			return false;
		}
		else {
			writer.addMessage(id, "in cache, requires validation");
			return true;
		}
	}

	// Check if response is valid in cache
	bool validInCache(int id, string req)
	{
		if (!last_modify.empty() && isEarlier(respMap[req].date_time, last_modify)) {
			return false;
		}
		if (!exp_time.empty() && isEarlier(respMap[req].exp_time, respMap[req].date_time)) {
			writer.addMessage(id, "in cache, but expired at" + respMap[req].exp_time);
			respMap.erase(req);
			return false;
		}
		return true;
	}

	// Compare time
	bool isEarlier(string s1, string s2) {
		struct tm tm1, tm2;
		if (strptime(s1.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm1) == NULL ||
			strptime(s2.c_str(), "%a, %d %b %Y %H:%M:%S GMT", &tm2) == NULL) {
			cerr << "strptime failed" << endl;
		}
		time_t t1 = mktime(&tm1);
		time_t t2 = mktime(&tm2);
		return difftime(t1, t2) < 0;
	}
	// check response no cache
	bool no_cache (string response, int ID) {
		if (response.find("200 OK") == string::npos) {
			writer.addMessage(ID, "not cacheable because don't recieve 200 OK");
			return true;
		}
		if (response.find("no-store") != string::npos) {
			writer.addMessage(ID, "not cacheable because no-store");
			return true;
		}
		return false;
	}
};



#endif