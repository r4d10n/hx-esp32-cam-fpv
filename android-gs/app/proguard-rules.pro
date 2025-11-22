# Add project specific ProGuard rules here.

# Keep decoder classes
-keep class com.hxesp32.fpvgs.video.** { *; }

# Keep MediaCodec related classes
-keep class android.media.MediaCodec { *; }
-keep class android.media.MediaFormat { *; }

# Kotlin
-keep class kotlin.** { *; }
-dontwarn kotlin.**
