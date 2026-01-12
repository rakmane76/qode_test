#include <gtest/gtest.h>
#include "common/config_parser.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

namespace mdfh {

class ConfigParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_dir_ = "test_config_temp";
        fs::create_directories(test_dir_);
    }

    void TearDown() override {
        if (fs::exists(test_dir_)) {
            fs::remove_all(test_dir_);
        }
    }

    // Helper to create a test config file
    std::string create_test_config(const std::string& content) {
        std::string config_file = test_dir_ + "/test.conf";
        std::ofstream file(config_file);
        file << content;
        file.close();
        return config_file;
    }

    std::string test_dir_;
};

// Test Case 1: Load Valid Config File
TEST_F(ConfigParserTest, LoadValidConfigFile) {
    std::string content = R"(
server.port=8080
server.host=localhost
market.num_symbols=100
market.tick_rate=1000
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    
    ASSERT_TRUE(parser.load(config_file));
    EXPECT_TRUE(parser.has_key("server.port"));
    EXPECT_TRUE(parser.has_key("server.host"));
    EXPECT_TRUE(parser.has_key("market.num_symbols"));
    EXPECT_TRUE(parser.has_key("market.tick_rate"));
}

// Test Case 2: Load Non-Existent File
TEST_F(ConfigParserTest, LoadNonExistentFile) {
    ConfigParser parser;
    EXPECT_FALSE(parser.load("nonexistent_config.conf"));
}

// Test Case 3: Get String Values
TEST_F(ConfigParserTest, GetStringValues) {
    std::string content = R"(
server.host=localhost
database.name=mydb
path=/usr/local/bin
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_string("server.host"), "localhost");
    EXPECT_EQ(parser.get_string("database.name"), "mydb");
    EXPECT_EQ(parser.get_string("path"), "/usr/local/bin");
}

// Test Case 4: Get String with Default Value
TEST_F(ConfigParserTest, GetStringWithDefault) {
    std::string content = "server.port=8080\n";
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_string("nonexistent.key", "default_value"), "default_value");
    EXPECT_EQ(parser.get_string("server.port", "default"), "8080");
}

// Test Case 5: Get Integer Values
TEST_F(ConfigParserTest, GetIntegerValues) {
    std::string content = R"(
server.port=8080
market.num_symbols=100
timeout=-1
zero_value=0
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_int("server.port"), 8080);
    EXPECT_EQ(parser.get_int("market.num_symbols"), 100);
    EXPECT_EQ(parser.get_int("timeout"), -1);
    EXPECT_EQ(parser.get_int("zero_value"), 0);
}

// Test Case 6: Get Integer with Default Value
TEST_F(ConfigParserTest, GetIntegerWithDefault) {
    std::string content = "server.port=8080\n";
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_int("nonexistent.key", 9999), 9999);
    EXPECT_EQ(parser.get_int("server.port", 1111), 8080);
}

// Test Case 7: Get Integer from Invalid String
TEST_F(ConfigParserTest, GetIntegerFromInvalidString) {
    std::string content = R"(
invalid_int=not_a_number
valid_int=42
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    // Should return default value for invalid integer
    EXPECT_EQ(parser.get_int("invalid_int", 999), 999);
    EXPECT_EQ(parser.get_int("valid_int", 0), 42);
}

// Test Case 8: Get Double Values
TEST_F(ConfigParserTest, GetDoubleValues) {
    std::string content = R"(
volatility=0.025
drift=-0.001
price=1234.56
scientific=1.23e-4
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_DOUBLE_EQ(parser.get_double("volatility"), 0.025);
    EXPECT_DOUBLE_EQ(parser.get_double("drift"), -0.001);
    EXPECT_DOUBLE_EQ(parser.get_double("price"), 1234.56);
    EXPECT_DOUBLE_EQ(parser.get_double("scientific"), 1.23e-4);
}

// Test Case 9: Get Double with Default Value
TEST_F(ConfigParserTest, GetDoubleWithDefault) {
    std::string content = "volatility=0.025\n";
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_DOUBLE_EQ(parser.get_double("nonexistent.key", 1.5), 1.5);
    EXPECT_DOUBLE_EQ(parser.get_double("volatility", 9.9), 0.025);
}

// Test Case 10: Get Double from Invalid String
TEST_F(ConfigParserTest, GetDoubleFromInvalidString) {
    std::string content = R"(
invalid_double=not_a_number
valid_double=3.14
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    // Should return default value for invalid double
    EXPECT_DOUBLE_EQ(parser.get_double("invalid_double", 99.9), 99.9);
    EXPECT_DOUBLE_EQ(parser.get_double("valid_double", 0.0), 3.14);
}

// Test Case 11: Get Boolean Values
TEST_F(ConfigParserTest, GetBooleanValues) {
    std::string content = R"(
bool_true=true
bool_false=false
bool_yes=yes
bool_no=no
bool_1=1
bool_0=0
bool_True=True
bool_FALSE=FALSE
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.get_bool("bool_true"));
    EXPECT_FALSE(parser.get_bool("bool_false"));
    EXPECT_TRUE(parser.get_bool("bool_yes"));
    EXPECT_FALSE(parser.get_bool("bool_no"));
    EXPECT_TRUE(parser.get_bool("bool_1"));
    EXPECT_FALSE(parser.get_bool("bool_0"));
    EXPECT_TRUE(parser.get_bool("bool_True"));
    EXPECT_FALSE(parser.get_bool("bool_FALSE"));
}

// Test Case 12: Get Boolean with Default Value
TEST_F(ConfigParserTest, GetBooleanWithDefault) {
    std::string content = "enabled=true\n";
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.get_bool("nonexistent.key", true));
    EXPECT_FALSE(parser.get_bool("nonexistent.key", false));
    EXPECT_TRUE(parser.get_bool("enabled", false));
}

// Test Case 13: Comments are Ignored
TEST_F(ConfigParserTest, CommentsAreIgnored) {
    std::string content = R"(
# This is a comment
server.port=8080
# Another comment
# database.host=old_value
server.host=localhost
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.has_key("server.port"));
    EXPECT_TRUE(parser.has_key("server.host"));
    EXPECT_FALSE(parser.has_key("database.host"));
}

// Test Case 14: Empty Lines are Ignored
TEST_F(ConfigParserTest, EmptyLinesAreIgnored) {
    std::string content = R"(

server.port=8080

server.host=localhost

)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.has_key("server.port"));
    EXPECT_TRUE(parser.has_key("server.host"));
}

// Test Case 15: Whitespace Trimming
TEST_F(ConfigParserTest, WhitespaceTrimming) {
    std::string content = R"(
  server.port  =  8080  
	server.host	=	localhost	
key_with_spaces   =   value_with_spaces   
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_string("server.port"), "8080");
    EXPECT_EQ(parser.get_string("server.host"), "localhost");
    EXPECT_EQ(parser.get_string("key_with_spaces"), "value_with_spaces");
    EXPECT_EQ(parser.get_int("server.port"), 8080);
}

// Test Case 16: Has Key Functionality
TEST_F(ConfigParserTest, HasKeyFunctionality) {
    std::string content = R"(
server.port=8080
market.enabled=true
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.has_key("server.port"));
    EXPECT_TRUE(parser.has_key("market.enabled"));
    EXPECT_FALSE(parser.has_key("nonexistent.key"));
    EXPECT_FALSE(parser.has_key("server"));
}

// Test Case 17: Lines Without Equals Sign are Ignored
TEST_F(ConfigParserTest, LinesWithoutEqualsAreIgnored) {
    std::string content = R"(
server.port=8080
invalid_line_no_equals
server.host=localhost
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.has_key("server.port"));
    EXPECT_TRUE(parser.has_key("server.host"));
    EXPECT_FALSE(parser.has_key("invalid_line_no_equals"));
}

// Test Case 18: Value Can Contain Equals Sign
TEST_F(ConfigParserTest, ValueCanContainEqualsSign) {
    std::string content = R"(
equation=a=b+c
url=http://example.com?param=value
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_EQ(parser.get_string("equation"), "a=b+c");
    EXPECT_EQ(parser.get_string("url"), "http://example.com?param=value");
}

// Test Case 19: Empty Values
TEST_F(ConfigParserTest, EmptyValues) {
    std::string content = R"(
empty_value=
server.port=8080
another_empty=
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    EXPECT_TRUE(parser.has_key("empty_value"));
    EXPECT_EQ(parser.get_string("empty_value"), "");
    EXPECT_EQ(parser.get_string("another_empty"), "");
    EXPECT_EQ(parser.get_int("empty_value", 42), 42);
}

// Test Case 20: Overwrite Duplicate Keys
TEST_F(ConfigParserTest, OverwriteDuplicateKeys) {
    std::string content = R"(
server.port=8080
server.port=9090
server.port=7070
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    parser.load(config_file);
    
    // Last value should win
    EXPECT_EQ(parser.get_int("server.port"), 7070);
}

// Test Case 21: Load Multiple Times
TEST_F(ConfigParserTest, LoadMultipleTimes) {
    std::string content1 = "server.port=8080\n";
    std::string content2 = "server.port=9090\nserver.host=localhost\n";
    
    std::string config_file1 = test_dir_ + "/config1.conf";
    std::string config_file2 = test_dir_ + "/config2.conf";
    
    std::ofstream file1(config_file1);
    file1 << content1;
    file1.close();
    
    std::ofstream file2(config_file2);
    file2 << content2;
    file2.close();
    
    ConfigParser parser;
    parser.load(config_file1);
    EXPECT_EQ(parser.get_int("server.port"), 8080);
    EXPECT_FALSE(parser.has_key("server.host"));
    
    // Load second config - should replace first
    parser.load(config_file2);
    EXPECT_EQ(parser.get_int("server.port"), 9090);
    EXPECT_TRUE(parser.has_key("server.host"));
}

// Test Case 22: Real-World Server Config
TEST_F(ConfigParserTest, RealWorldServerConfig) {
    std::string content = R"(
# Server Configuration
server.port=9876
server.host=0.0.0.0
server.backlog=128

# Market Data Configuration
market.num_symbols=50
market.tick_rate=100000
market.symbols_file=config/symbols.csv

# Performance Tuning
performance.thread_pool_size=4
performance.buffer_size=65536
performance.use_huge_pages=true

# Fault Injection
fault_injection.enabled=false
fault_injection.drop_rate=0.01
)";
    
    std::string config_file = create_test_config(content);
    ConfigParser parser;
    ASSERT_TRUE(parser.load(config_file));
    
    EXPECT_EQ(parser.get_int("server.port"), 9876);
    EXPECT_EQ(parser.get_string("server.host"), "0.0.0.0");
    EXPECT_EQ(parser.get_int("server.backlog"), 128);
    
    EXPECT_EQ(parser.get_int("market.num_symbols"), 50);
    EXPECT_EQ(parser.get_int("market.tick_rate"), 100000);
    EXPECT_EQ(parser.get_string("market.symbols_file"), "config/symbols.csv");
    
    EXPECT_EQ(parser.get_int("performance.thread_pool_size"), 4);
    EXPECT_EQ(parser.get_int("performance.buffer_size"), 65536);
    EXPECT_TRUE(parser.get_bool("performance.use_huge_pages"));
    
    EXPECT_FALSE(parser.get_bool("fault_injection.enabled"));
    EXPECT_DOUBLE_EQ(parser.get_double("fault_injection.drop_rate"), 0.01);
}

} // namespace mdfh

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
