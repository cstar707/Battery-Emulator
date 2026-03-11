#include <gtest/gtest.h>

#include "../Software/src/devboard/utils/release_metadata.h"

TEST(ReleaseMetadataTests, IdentifierStartsWithSemanticVersion) {
  const auto identifier = release_metadata::release_identifier();

  EXPECT_FALSE(identifier.empty());
  EXPECT_EQ(identifier.rfind(release_metadata::kVersion, 0), 0U);
}

TEST(ReleaseMetadataTests, SummaryIncludesBuildId) {
  const auto summary = release_metadata::release_summary();

  EXPECT_NE(summary.find(release_metadata::kBuildId), std::string::npos);
  EXPECT_NE(summary.find(release_metadata::kVersion), std::string::npos);
}