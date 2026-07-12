variable "TAG" {
  default = "latest"
}

variable "REGISTRY" {
  default = ""
  description = "Container registry (e.g. ghcr.io/airprofly/backer). Empty = Docker Hub."
}

variable "GH_PROXY" {
  default = ""
  description = "GitHub proxy mirror for Chinese users (e.g. https://ghproxy.net/)"
}

target "default" {
  dockerfile = "Dockerfile"
  tags = [
    "${REGISTRY != "" ? REGISTRY : "backer"}:${TAG}",
    "${REGISTRY != "" ? REGISTRY : "backer"}:latest"
  ]
  platforms = ["linux/amd64", "linux/arm64"]
  args = {
    GH_PROXY = GH_PROXY
  }
}
