plugins {
    java
}

group = "software.aws.rds"
version = "1.0-SNAPSHOT"

repositories {
    mavenCentral()
}

dependencies {
    testImplementation("software.amazon.awssdk:rds:2.25.36")
    testImplementation("software.amazon.awssdk:ec2:2.25.36")
    testImplementation("software.amazon.awssdk:secretsmanager:2.25.36")
    testImplementation("org.junit.jupiter:junit-jupiter-api:5.8.2")
    testImplementation("org.testcontainers:toxiproxy:1.19.8")
    testImplementation("org.testcontainers:mysql:1.19.8")
    testImplementation("org.json:json:20231013")

    testRuntimeOnly("org.junit.jupiter:junit-jupiter-engine")
}

tasks.getByName<Test>("test") {
    useJUnitPlatform()
}

tasks.register<Test>("test-failover") {
    filter.includeTestsMatching("host.IntegrationContainerTest.testRunFailoverTestInContainer")
}

tasks.register<Test>("test-community") {
    filter.includeTestsMatching("host.IntegrationContainerTest.testRunCommunityTestInContainer")
}

tasks.register<Test>("test-performance") {
    filter.includeTestsMatching("host.IntegrationContainerTest.testRunPerformanceTestInContainer")
}

tasks.withType<Test> {
    useJUnitPlatform()
    group = "verification"
    this.testLogging {
        this.showStandardStreams = true
    }
}
