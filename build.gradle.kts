fun String.execute(currentWorkingDir: File = file("./")): String {
    val byteOut = java.io.ByteArrayOutputStream()
    project.exec {
        workingDir = currentWorkingDir
        commandLine = split("\\s".toRegex())
        standardOutput = byteOut
    }
    return String(byteOut.toByteArray()).trim()
}

val minSdkVer by extra(24)
val targetSdkVer by extra(31)
val buildToolsVer by extra("31.0.0")

val appVerName by extra("2.2.4")
val appVerCode by extra(70)
val serviceVer by extra(69)
val minRiruVer by extra(29)
val minBackupVer by extra(65)

val gitCommitCount by extra("git rev-list HEAD --count".execute())
val gitCommitHash by extra("git rev-parse --verify --short HEAD".execute())

tasks.register("clean", Delete::class) {
    delete(rootProject.buildDir)
}