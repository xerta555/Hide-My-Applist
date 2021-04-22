#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <queue>
#include <regex>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include "dobby.h"
#include "json.hpp"

using std::string;

constexpr char APPNAME[] = "com.tsng.hidemyapplist";

struct Preference {
    struct Template {
        bool HideTWRP = false;
        bool HideAllApps = false;
        bool EnableAllHooks = false;
        bool ExcludeWebview = false;
        std::vector<string> HideApps;
        std::vector<string> ApplyHooks;
    };

    bool HookSelf = false;
    bool DetailLog = false;
    std::map<string, string> Scope;
    std::map<string, Template> Templates;
} data;

template<>
struct jsonxx::json_bind<Preference::Template> {
    static void from_json(const json &j, Preference::Template &v) {
        jsonxx::from_json(j["HideTWRP"], v.HideTWRP);
        jsonxx::from_json(j["HideAllApps"], v.HideAllApps);
        jsonxx::from_json(j["EnableAllHooks"], v.EnableAllHooks);
        jsonxx::from_json(j["ExcludeWebview"], v.ExcludeWebview);
        jsonxx::from_json(j["HideApps"], v.HideApps);
        jsonxx::from_json(j["ApplyHooks"], v.ApplyHooks);
    }
};

template<>
struct jsonxx::json_bind<Preference> {
    static void from_json(const json &j, Preference &v) {
        jsonxx::from_json(j["HookSelf"], v.HookSelf);
        jsonxx::from_json(j["DetailLog"], v.DetailLog);
        jsonxx::from_json(j["Scope"], v.Scope);
        jsonxx::from_json(j["Templates"], v.Templates);
    }
};

const char *callerName;
std::queue<string> messageQueue;

void ld(const string &s) {
    messageQueue.push("DEBUG");
    messageQueue.push(s);
}

void li(const string &s) {
    messageQueue.push("INFO");
    messageQueue.push(s);
}

void le(const string &s) {
    messageQueue.push("ERROR");
    messageQueue.push(s);
}

bool isUseHook(const string &hookMethod) {
    if (strcmp(callerName, APPNAME) == 0 && !data.HookSelf) return false;
    if (!data.Scope.count(callerName)) return false;
    const auto &tplName = data.Scope[callerName];
    if (!data.Templates.count(tplName)) return false;
    const auto &tpl = data.Templates[tplName];
    return tpl.EnableAllHooks |
           std::find(tpl.ApplyHooks.begin(), tpl.ApplyHooks.end(), hookMethod) !=
           tpl.ApplyHooks.end();
}

bool isHideFile(const char *path) {
    if (callerName == nullptr || path == nullptr) return false;
    if (strstr(path, callerName) != nullptr) return false;
    if (!data.Scope.count(callerName)) return false;
    const auto &tplName = data.Scope[callerName];
    if (!data.Templates.count(tplName)) return false;
    const auto &tpl = data.Templates[tplName];
    if (tpl.ExcludeWebview &&
        std::regex_search(path, std::regex("[Ww]ebview")))
        return false;
    if (tpl.HideTWRP &&
        std::regex_search(path, std::regex("/storage/emulated/(.*)/TWRP")))
        return true;
    if (tpl.HideAllApps &&
        std::regex_search(path, std::regex("/storage/emulated/(.*)/Android/data/")))
        return true;
    for (const auto &pkg : tpl.HideApps)
        if (strstr(path, pkg.c_str()) != nullptr)
            return true;
    return false;
}

int (*orig_access)(const char *path, int mode);
int fake_access(const char *path, int mode) {
    if (isUseHook("File detections") && isHideFile(path)) {
        std::stringstream message;
        message << "@Hide nativeAccess caller: " << callerName << " param: " << path;
        li(message.str());
        return -1;
    }
    return orig_access(path, mode);
}

int (*orig_stat)(const char *path, struct stat *buf);
int fake_stat(const char *path, struct stat *buf) {
    if (isUseHook("File detections") && isHideFile(path)) {
        std::stringstream message;
        message << "@Hide nativeStat caller: " << callerName << " param: " << path;
        li(message.str());
        return -1;
    }
    return orig_stat(path, buf);
}

int (*orig_open)(const char *path, int flags, ...);
int fake_open(const char *path, int flags, ...) {
    if (isUseHook("File detections") && isHideFile(path)) {
        std::stringstream message;
        message << "@Hide nativeOpen caller: " << callerName << " param: " << path;
        li(message.str());
        return -1;
    }
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = (mode_t) va_arg(args, int);
        va_end(args);
    }
    return orig_open(path, flags, mode);
}

extern "C" JNIEXPORT void JNICALL
Java_com_tsng_hidemyapplist_xposed_hooks_IndividualHooks_initNative(JNIEnv *env, jobject, jstring j_pkgName) {
    callerName = env->GetStringUTFChars(j_pkgName, nullptr);
    DobbyHook((void *) access, (void *) fake_access, (void **) &orig_access);
    DobbyHook((void *) stat, (void *) fake_stat, (void **) &orig_stat);
    int (*p_orig_open)(const char *, int) = __open_2;
    DobbyHook((void *) p_orig_open, (void *) fake_open, (void **) &orig_open);
    DobbyHook((void *) DobbySymbolResolver(nullptr, "open"), (void *) fake_open, (void **) &orig_open);
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_com_tsng_hidemyapplist_xposed_hooks_IndividualHooks_nativeBridge(JNIEnv *env, jobject, jstring j_json) {
    jsonxx::from_json(jsonxx::json::parse(env->GetStringUTFChars(j_json, nullptr)), data);
    int length = messageQueue.size();
    jobjectArray ret = env->NewObjectArray(length, env->FindClass("java/lang/String"), nullptr);
    for (int i = 0; i < length; i++) {
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(messageQueue.front().c_str()));
        messageQueue.pop();
    }
    return ret;
}