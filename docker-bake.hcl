variable "TAG" {
  default = "latest"
}

variable "REGISTRY" {
  default = "ghcr.io/airprofly/backer"
}

variable "GH_PROXY" {
  default = ""
  description = "GitHub proxy mirror for Chinese users (e.g. https://ghproxy.net/)"
}

target "default" {
  dockerfile = "Dockerfile"
  tags = ["${REGISTRY}:${TAG}"]
  platforms = ["linux/amd64", "linux/arm64"]
  args = {
    GH_PROXY = GH_PROXY
  }
}
