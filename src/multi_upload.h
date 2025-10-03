#pragma once
#include "uploader.h"

class MultiUpload : public Uploader {
  public:
    MultiUpload(Uploader* a, Uploader* b) : a_(a), b_(b) {}
    void Setup() override { if (a_) a_->Setup(); if (b_) b_->Setup(); }
    void notifyClients(const String &m) override { if (a_) a_->notifyClients(m); if (b_) b_->notifyClients(m); }
    void loop() override { if (a_) a_->loop(); if (b_) b_->loop(); }
  private:
    Uploader* a_;
    Uploader* b_;
};
