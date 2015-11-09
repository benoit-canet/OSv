#include <cassert>
#include <iostream>
#include <thread>

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
    std::cout << "start env" << std::endl;
    std::thread t = std::thread(test_env);
    t.join();
    std::cout << "end env" << std::endl;

    char *value = getenv("FOO");
    assert(!value);
}

int main(int argc, char **argv)
{
    with_namespaces();

    env_with_namespaces();

    return 0;
}
