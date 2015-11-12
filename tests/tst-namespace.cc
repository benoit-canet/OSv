#include <cassert>
#include <iostream>
#include <thread>
#include <map>
#include <string>

#include <osv/run.hh>

void run_namespace()
{
    std::vector<std::string> args;
    std::shared_ptr<osv::application> app;
    int ret;

    args.push_back("tests/payload-namespace.so");

    app = osv::run("/tests/payload-namespace.so", args, &ret, true);
    assert(!ret);

}

void with_namespaces()
{
    std::thread first = std::thread(run_namespace);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::thread second = std::thread(run_namespace);
    first.join();
    second.join();
}

void test_env()
{
    std::vector<std::string> args;
    std::shared_ptr<osv::application> app;
    int ret;

    args.push_back("tests/payload-env-namespace.so");

    app = osv::run("/tests/payload-env-namespace.so", args, &ret, true);
    assert(!ret);
}


void env_with_namespaces()
{
    std::thread t = std::thread(test_env);
    t.join();

    char *value = getenv("FOO");
    assert(!value);
}

void test_overwrite_env()
{
    std::vector<std::string> args;
    std::map<std::string, std::string> env;
    std::shared_ptr<osv::application> app;
    int ret;

    env["SECOND"] = "MOUCHE";
    env["THIRD"] = "COCCINELLE";

    args.push_back("tests/payload-overwrite-env-namespace.so");

    app = osv::run("/tests/payload-overwrite-env-namespace.so",
                   args, &ret, true, env);
    assert(!ret);
}


void overwrite_env_namespaces()
{
    setenv("FIRST", "GATEAU", 1);
    setenv("SECOND", "CHAMEAU", 1);
    std::thread t = std::thread(test_overwrite_env);
    t.join();
}

int main(int argc, char **argv)
{
    with_namespaces();

    env_with_namespaces();

    overwrite_env_namespaces();

    return 0;
}
