import java.util.Properties
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
    id("org.jetbrains.kotlin.plugin.compose")
}

// Topic and key are shared with the ESP, see tools/widget_secret.py.
val secretsFile = rootProject.file("secrets.properties")
if (!secretsFile.exists()) {
    throw GradleException("android/secrets.properties is missing, run tools/widget_secret.py")
}
val secrets = Properties().apply { secretsFile.inputStream().use(::load) }

android {
    namespace = "com.pumpkin.wolwidget"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.pumpkin.wolwidget"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "1.0"

        buildConfigField("String", "NTFY_TOPIC", "\"${secrets.getProperty("topic")}\"")
        buildConfigField("String", "WIDGET_KEY", "\"${secrets.getProperty("key")}\"")
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}

kotlin {
    compilerOptions {
        jvmTarget.set(JvmTarget.JVM_17)
    }
}

dependencies {
    implementation(platform("androidx.compose:compose-bom:2025.12.01"))
    implementation("androidx.compose.material3:material3")
    implementation("androidx.activity:activity-compose:1.12.4")
    implementation("androidx.glance:glance-appwidget:1.1.1")
}
