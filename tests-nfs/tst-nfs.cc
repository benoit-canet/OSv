#include <cassert>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <fcntl.h>

#include <string>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

void assert_mount_error(int ret)
{
    assert(ret != EACCES);
    assert(ret != EBUSY);
    assert(ret != EFAULT);
    assert(ret != EINVAL);
    assert(ret != ELOOP);
    assert(ret != EMFILE);
    assert(ret != ENAMETOOLONG);
    assert(ret != ENODEV);
    assert(ret != ENOENT);
    assert(ret != ENOMEM);
    assert(ret != ENOTBLK);
    assert(ret != ENOTDIR);
    assert(ret != ENXIO);
    assert(ret != EPERM);
}

static void test_bogus_url_mount(std::string server, std::string share)
{
    std::string mount_point("/bogus");
    mkdir(mount_point.c_str(), 0777);
    // Did you notice OSv didn't supported gluster ?
    std::string url("gluster://" + server + share);
    int ret = mount(url.c_str(), mount_point.c_str(), "nfs", 0, nullptr);
    assert(ret && errno == EINVAL);
}

static void test_mount(std::string server, std::string share,
                       std::string mount_point)
{
    mkdir(mount_point.c_str(), 0777);
    std::string url("nfs://" + server + share);
    int ret = mount(url.c_str(), mount_point.c_str(), "nfs", 0, nullptr);
    if (ret) {
        int my_errno = errno;
        std::cout << "Error: " << strerror(my_errno) << "(" << my_errno << ")"
                  << std::endl;
        assert_mount_error(my_errno);
    }
    assert(!ret);
}

static void test_umount(std::string &mount_point)
{
    int ret = umount(mount_point.c_str());
    assert(!ret);
}

static void test_mkdir(std::string mount_point, std::string dir)
{
    std::string path = mount_point + "/" + dir;
    int ret = mkdir(path.c_str(), 0755);
    assert(!ret);
}

static void test_rmdir(std::string mount_point, std::string dir,
                       int result, int err_no)
{
    std::string path = mount_point + "/" + dir;
    int ret = rmdir(path.c_str());
    assert(ret == result);
    if (ret) {
        assert(err_no = errno);
    }
}

static void test_creat_and_close(std::string mount_point, std::string path)
{
    std::string full_path = mount_point + "/" + path;
    int fd = creat(full_path.c_str(), 0500);
    assert(fd != -1);
    int ret = close(fd);
    assert(!ret);
}

int main(int argc, char **argv)
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("server", po::value<std::string>(), "set server ip")
        ("share", po::value<std::string>(), "set remote share")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    std::string server;
    if (vm.count("server")) {
        server = vm["server"].as<std::string>();
    } else {
        std::cout << desc << std::endl;
        return 1;
    }

    std::string share;
    if (vm.count("share")) {
        share = vm["share"].as<std::string>();
    } else {
        std::cout << desc << std::endl;
        return 1;
    }

    test_bogus_url_mount(server, share);

    std::string mount_point("/nfs");

    // Testing mount/umount
    test_mount(server, share, mount_point);
    test_umount(mount_point);

    // Testing mkdir and rmdir
    test_mount(server, share, mount_point);
    // Test to rmdir something not existing
    test_rmdir(mount_point, "bar", -1, ENOENT);
    // mkdir followed by rmdir
    test_mkdir(mount_point, "foo");
    test_rmdir(mount_point, "foo", 0, 0);
    test_umount(mount_point);

    test_mkdir(mount_point, "blub");
    // Testing creat and close
    test_creat_and_close(mount_point, "zorglub");

    return 0;
}
