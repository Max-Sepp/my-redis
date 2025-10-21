#include <gtest/gtest.h>

#include "parser/Parser.h"

class RespParsingTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Simple String Tests
TEST_F(RespParsingTest, ParseSimpleString) {
  const RespValue resp("+OK\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleString>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleString>(value), "OK");
}

TEST_F(RespParsingTest, ParseSimpleStringEmpty) {
  const RespValue resp("+\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleString>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleString>(value), "");
}

TEST_F(RespParsingTest, ParseSimpleStringWithSpaces) {
  const RespValue resp("+Hello World\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleString>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleString>(value), "Hello World");
}

// Simple Error Tests
TEST_F(RespParsingTest, ParseSimpleError) {
  const RespValue resp("-Error message\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleError>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleError>(value).message,
            "Error message");
}

TEST_F(RespParsingTest, ParseSimpleErrorEmpty) {
  const RespValue resp("-\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleError>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleError>(value).message, "");
}

TEST_F(RespParsingTest, ParseSimpleErrorComplex) {
  const RespValue resp("-ERR unknown command 'foobar'\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespSimpleError>(value));
  EXPECT_EQ(std::get<RespValue::RespSimpleError>(value).message,
            "ERR unknown command 'foobar'");
}

// Integer Tests
TEST_F(RespParsingTest, ParseIntegerPositive) {
  const RespValue resp(":1000\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(value));
  EXPECT_EQ(std::get<RespValue::RespInteger>(value), 1000);
}

TEST_F(RespParsingTest, ParseIntegerNegative) {
  const RespValue resp(":-1000\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(value));
  EXPECT_EQ(std::get<RespValue::RespInteger>(value), -1000);
}

TEST_F(RespParsingTest, ParseIntegerZero) {
  const RespValue resp(":0\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(value));
  EXPECT_EQ(std::get<RespValue::RespInteger>(value), 0);
}

TEST_F(RespParsingTest, ParseIntegerMaxValue) {
  const RespValue resp(":9223372036854775807\r\n");  // max int64_t
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(value));
  EXPECT_EQ(std::get<RespValue::RespInteger>(value), 9223372036854775807LL);
}

TEST_F(RespParsingTest, ParseIntegerMinValue) {
  const RespValue resp(":-922337203685477580\r\n");  // min int64_t
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(value));
  EXPECT_EQ(std::get<RespValue::RespInteger>(value), -922337203685477580LL);
}

// Bulk String Tests
TEST_F(RespParsingTest, ParseBulkStringNormal) {
  const RespValue resp("$6\r\nfoobar\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(value));
  const auto& bulkString = std::get<RespValue::RespBulkString>(value);
  ASSERT_TRUE(bulkString.has_value());
  EXPECT_EQ(bulkString.value(), "foobar");
}

TEST_F(RespParsingTest, ParseBulkStringEmpty) {
  const RespValue resp("$0\r\n\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(value));
  const auto& bulkString = std::get<RespValue::RespBulkString>(value);
  ASSERT_TRUE(bulkString.has_value());
  EXPECT_EQ(bulkString.value(), "");
}

TEST_F(RespParsingTest, ParseBulkStringNull) {
  const RespValue resp("$-1\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(value));
  const auto& bulkString = std::get<RespValue::RespBulkString>(value);
  EXPECT_FALSE(bulkString.has_value());
}

TEST_F(RespParsingTest, ParseBulkStringWithNewlines) {
  const RespValue resp("$12\r\nhello\r\nworld\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(value));
  const auto& bulkString = std::get<RespValue::RespBulkString>(value);
  ASSERT_TRUE(bulkString.has_value());
  EXPECT_EQ(bulkString.value(), "hello\r\nworld");
}

// Array Tests
TEST_F(RespParsingTest, ParseArrayEmpty) {
  const RespValue resp("*0\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(value));
  const auto& array = std::get<RespValue::RespArray>(value);
  EXPECT_EQ(array.size(), 0);
}

TEST_F(RespParsingTest, ParseArrayTwoStrings) {
  const RespValue resp("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(value));
  const auto& array = std::get<RespValue::RespArray>(value);
  ASSERT_EQ(array.size(), 2);

  // First element
  const auto& first = array[0].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(first));
  EXPECT_EQ(std::get<RespValue::RespBulkString>(first).value(), "foo");

  // Second element
  const auto& second = array[1].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(second));
  EXPECT_EQ(std::get<RespValue::RespBulkString>(second).value(), "bar");
}

TEST_F(RespParsingTest, ParseArrayMixedTypes) {
  const RespValue resp("*3\r\n:1\r\n:2\r\n:3\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(value));
  const auto& array = std::get<RespValue::RespArray>(value);
  ASSERT_EQ(array.size(), 3);

  for (int i = 0; i < 3; ++i) {
    const auto& element = array[i].getValue();
    ASSERT_TRUE(std::holds_alternative<RespValue::RespInteger>(element));
    EXPECT_EQ(std::get<RespValue::RespInteger>(element), i + 1);
  }
}

TEST_F(RespParsingTest, ParseArrayWithNullElements) {
  RespValue resp("*3\r\n$3\r\nfoo\r\n$-1\r\n$3\r\nbar\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(value));
  const auto& array = std::get<RespValue::RespArray>(value);
  ASSERT_EQ(array.size(), 3);

  // First element
  const auto& first = array[0].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(first));
  EXPECT_EQ(std::get<RespValue::RespBulkString>(first).value(), "foo");

  // Second element (null)
  const auto& second = array[1].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(second));
  EXPECT_FALSE(std::get<RespValue::RespBulkString>(second).has_value());

  // Third element
  const auto& third = array[2].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespBulkString>(third));
  EXPECT_EQ(std::get<RespValue::RespBulkString>(third).value(), "bar");
}

TEST_F(RespParsingTest, ParseNestedArray) {
  const RespValue resp("*2\r\n*3\r\n:1\r\n:2\r\n:3\r\n*2\r\n+Foo\r\n-Bar\r\n");
  const auto& value = resp.getValue();

  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(value));
  const auto& outerArray = std::get<RespValue::RespArray>(value);
  ASSERT_EQ(outerArray.size(), 2);

  // First nested array
  const auto& first = outerArray[0].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(first));
  const auto& firstArray = std::get<RespValue::RespArray>(first);
  ASSERT_EQ(firstArray.size(), 3);

  // Second nested array
  const auto& second = outerArray[1].getValue();
  ASSERT_TRUE(std::holds_alternative<RespValue::RespArray>(second));
  const auto& secondArray = std::get<RespValue::RespArray>(second);
  ASSERT_EQ(secondArray.size(), 2);
}

// Edge Cases and Error Handling
TEST_F(RespParsingTest, ParseInvalidTypePrefix) {
  EXPECT_THROW(RespValue("@invalid\r\n"), std::invalid_argument);
}

TEST_F(RespParsingTest, ParseMissingCRLF) {
  EXPECT_THROW(RespValue("+OK"), std::invalid_argument);
}

TEST_F(RespParsingTest, ParseInvalidInteger) {
  EXPECT_THROW(RespValue(":not_a_number\r\n"), std::invalid_argument);
}

TEST_F(RespParsingTest, ParseBulkStringLengthMismatch) {
  EXPECT_THROW(RespValue("$6\r\nfoo\r\n"),
               std::invalid_argument);  // length says 6 but only 3 chars
}

TEST_F(RespParsingTest, ParseNegativeBulkStringLengthNotMinusOne) {
  EXPECT_THROW(RespValue("$-2\r\n"), std::invalid_argument);
}

TEST_F(RespParsingTest, ParseArrayCountMismatch) {
  EXPECT_THROW(RespValue("*2\r\n$3\r\nfoo\r\n"),
               std::invalid_argument);  // says 2 elements but only 1
}

class RespSerializeTests : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
};

// Simple String
TEST(RespSerializeTests, SimpleString) {
  const RespValue value("+OK\r\n");
  EXPECT_EQ(value.serialize(), "+OK\r\n");
}

// Simple Error
TEST(RespSerializeTests, SimpleError) {
  const RespValue value(RespValue::RespSimpleError{"ERR unknown command"});
  EXPECT_EQ(value.serialize(), "-ERR unknown command\r\n");
}

// Integer
TEST(RespSerializeTests, Integer) {
  const RespValue value(static_cast<RespValue::RespInteger>(42));
  EXPECT_EQ(value.serialize(), ":42\r\n");
}

// Bulk String (non-null)
TEST(RespSerializeTests, BulkString) {
  const RespValue value(
      RespValue::RespBulkString(std::make_optional<std::string>("foobar")));
  EXPECT_EQ(value.serialize(), "$6\r\nfoobar\r\n");
}

// Bulk String (null)
TEST(RespSerializeTests, BulkStringNull) {
  const RespValue value{RespValue::RespBulkString(std::nullopt)};
  EXPECT_EQ(value.serialize(), "$-1\r\n");
}

// Array (empty)
TEST(RespSerializeTests, ArrayEmpty) {
  const RespValue value(RespValue::RespArray{});
  EXPECT_EQ(value.serialize(), "*0\r\n");
}

// Array (with elements)
TEST(RespSerializeTests, ArrayWithElements) {
  RespValue::RespArray arr;
  arr.emplace_back("+foo\r\n");
  arr.emplace_back(static_cast<RespValue::RespInteger>(123));
  arr.emplace_back(
      RespValue::RespBulkString(std::make_optional<std::string>("bar")));
  const RespValue value(arr);
  EXPECT_EQ(value.serialize(), "*3\r\n+foo\r\n:123\r\n$3\r\nbar\r\n");
}
