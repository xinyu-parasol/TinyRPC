#include "EtcdClient.h"
#include <curl/curl.h>
#include <cstring>
#include <cstdint>
#include <sstream>

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, std::string *output) {
    size_t total = size * nmemb;
    output->append(static_cast<char *>(contents), total);
    return total;
}

static int Base64Index(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

std::string EtcdClient::Base64Encode(const std::string &data) {
    std::string result;
    size_t i = 0;
    unsigned char buf3[3], buf4[4];

    for (unsigned char c : data) {
        buf3[i++] = c;
        if (i == 3) {
            buf4[0] = (buf3[0] & 0xfc) >> 2;
            buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
            buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
            buf4[3] = buf3[2] & 0x3f;
            for (int j = 0; j < 4; ++j) result += kBase64Chars[buf4[j]];
            i = 0;
        }
    }

    if (i > 0) {
        for (size_t j = i; j < 3; ++j) buf3[j] = 0;
        buf4[0] = (buf3[0] & 0xfc) >> 2;
        buf4[1] = ((buf3[0] & 0x03) << 4) | ((buf3[1] & 0xf0) >> 4);
        buf4[2] = ((buf3[1] & 0x0f) << 2) | ((buf3[2] & 0xc0) >> 6);
        buf4[3] = buf3[2] & 0x3f;
        for (size_t j = 0; j < i + 1; ++j) result += kBase64Chars[buf4[j]];
        while (i++ < 3) result += '=';
    }
    return result;
}

std::string EtcdClient::Base64Decode(const std::string &data) {
    std::string result;
    size_t i = 0;
    unsigned char buf4[4];

    for (char c : data) {
        if (c == '=') break;
        int idx = Base64Index(c);
        if (idx < 0) continue;
        buf4[i++] = idx;
        if (i == 4) {
            result += (buf4[0] << 2) | ((buf4[1] & 0x30) >> 4);
            result += ((buf4[1] & 0x0f) << 4) | ((buf4[2] & 0x3c) >> 2);
            result += ((buf4[2] & 0x03) << 6) | buf4[3];
            i = 0;
        }
    }
    if (i >= 2) {
        result += (buf4[0] << 2) | ((buf4[1] & 0x30) >> 4);
    }
    if (i >= 3) {
        result += ((buf4[1] & 0x0f) << 4) | ((buf4[2] & 0x3c) >> 2);
    }
    return result;
}

std::string EtcdClient::Uint64ToString(uint64_t v) {
    std::ostringstream oss;
    oss << v;
    return oss.str();
}

static std::string ExtractJsonString(const std::string &json, const std::string &key) {
    std::string target = "\"" + key + "\":\"";
    size_t pos = json.find(target);
    if (pos == std::string::npos) return "";

    pos += target.size();
    std::string value;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
        }
        value += json[pos++];
    }
    return value;
}

EtcdClient::EtcdClient(const std::string &etcd_ip, uint16_t etcd_port) {
    static bool curl_inited = false;
    if (!curl_inited) {
        curl_global_init(CURL_GLOBAL_ALL);
        curl_inited = true;
    }
    base_url_ = "http://" + etcd_ip + ":" + std::to_string(etcd_port);
}

std::string EtcdClient::HttpPost(const std::string &path, const std::string &json_body) {
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = base_url_ + path;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "[EtcdClient] POST %s error: %d %s (url=%s, body=%s)\n", path.c_str(),
                res, curl_easy_strerror(res), url.c_str(), json_body.c_str());
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

std::string EtcdClient::HttpPut(const std::string &path, const std::string &json_body) {
    CURL *curl = curl_easy_init();
    if (!curl) return "";

    std::string response;
    std::string url = base_url_ + path;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "[EtcdClient] PUT %s error: %s\n", path.c_str(),
                curl_easy_strerror(res));
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return response;
}

// ---- etcd v3 API methods ----

int64_t EtcdClient::GrantLease(int ttl_sec) {
    std::string body = "{\"TTL\":" + std::to_string(ttl_sec) + "}";
    std::string resp = HttpPost("/v3/lease/grant", body);
    if (resp.empty()) return 0;

    std::string id_str = ExtractJsonString(resp, "ID");
    if (id_str.empty()) return 0;

    return std::stoll(id_str);
}

bool EtcdClient::KeepAliveLease(int64_t lease_id) {
    std::string body = "{\"ID\":" + Uint64ToString(static_cast<uint64_t>(lease_id)) + "}";
    std::string resp = HttpPost("/v3/lease/keepalive", body);
    return !resp.empty() && resp.find("error") == std::string::npos;
}

bool EtcdClient::RevokeLease(int64_t lease_id) {
    std::string body = "{\"ID\":" + Uint64ToString(static_cast<uint64_t>(lease_id)) + "}";
    std::string resp = HttpPost("/v3/lease/revoke", body);
    return !resp.empty();
}

bool EtcdClient::Put(const std::string &key, const std::string &value, int64_t lease_id) {
    std::string body = "{\"key\":\"" + Base64Encode(key) + "\",\"value\":\"" + Base64Encode(value) + "\"";
    if (lease_id > 0) {
        body += ",\"lease\":" + Uint64ToString(static_cast<uint64_t>(lease_id));
    }
    body += "}";

    std::string resp = HttpPost("/v3/kv/put", body);
    return !resp.empty() && resp.find("error") == std::string::npos;
}

std::string EtcdClient::Get(const std::string &key) {
    std::string body = "{\"key\":\"" + Base64Encode(key) + "\"}";
    std::string resp = HttpPost("/v3/kv/range", body);
    if (resp.empty()) return "";

    std::string value_b64 = ExtractJsonString(resp, "value");
    if (value_b64.empty()) return "";

    return Base64Decode(value_b64);
}

bool EtcdClient::Delete(const std::string &key) {
    std::string body = "{\"key\":\"" + Base64Encode(key) + "\"}";
    std::string resp = HttpPost("/v3/kv/deleterange", body);
    return !resp.empty();
}

// ---- High-level service registration ----

bool EtcdClient::RegisterService(const std::string &key, const std::string &value, int ttl_sec) {
    int64_t lease_id = GrantLease(ttl_sec);
    if (lease_id == 0) {
        fprintf(stderr, "[EtcdClient] Failed to grant lease for %s\n", key.c_str());
        return false;
    }
    if (!Put(key, value, lease_id)) {
        fprintf(stderr, "[EtcdClient] Failed to put key %s\n", key.c_str());
        RevokeLease(lease_id);
        return false;
    }
    active_leases_[key] = {lease_id, ttl_sec};
    printf("[EtcdClient] Registered %s -> %s (lease=%ld, TTL=%ds)\n",
           key.c_str(), value.c_str(), static_cast<long>(lease_id), ttl_sec);
    return true;
}

bool EtcdClient::UnregisterService(const std::string &key) {
    auto it = active_leases_.find(key);
    if (it != active_leases_.end()) {
        RevokeLease(it->second.lease_id);
        active_leases_.erase(it);
    }
    Delete(key);
    printf("[EtcdClient] Unregistered %s\n", key.c_str());
    return true;
}

std::string EtcdClient::DiscoverService(const std::string &key) {
    return Get(key);
}

void EtcdClient::KeepAliveAll() {
    for (auto &entry : active_leases_) {
        KeepAliveLease(entry.second.lease_id);
    }
}
