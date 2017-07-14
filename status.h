/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 ==============================================================================*/

#ifndef STATUS_H_
#define STATUS_H_

#include <iostream>
#include <string>
#include <sstream>

namespace error {
  enum Code {
    // Not an error; returned on success
    OK = 0,

    // The operation was cancelled (typically by the caller).
    CANCELLED = 1,

    // Unknown error.  An example of where this error may be returned is
    // if a Status value received from another address space belongs to
    // an error-space that is not known in this address space.  Also
    // errors raised by APIs that do not return enough error information
    // may be converted to this error.
    UNKNOWN = 2,

    // Client specified an invalid argument.  Note that this differs
    // from FAILED_PRECONDITION.  INVALID_ARGUMENT indicates arguments
    // that are problematic regardless of the state of the system
    // (e.g., a malformed file name).
    INVALID_ARGUMENT = 3,

    // Deadline expired before operation could complete.  For operations
    // that change the state of the system, this error may be returned
    // even if the operation has completed successfully.  For example, a
    // successful response from a server could have been delayed long
    // enough for the deadline to expire.
    DEADLINE_EXCEEDED = 4,

    // Some requested entity (e.g., file or directory) was not found.
    // For privacy reasons, this code *may* be returned when the client
    // does not have the access right to the entity.
    NOT_FOUND = 5,

    // Some entity that we attempted to create (e.g., file or directory)
    // already exists.
    ALREADY_EXISTS = 6,

    // The caller does not have permission to execute the specified
    // operation.  PERMISSION_DENIED must not be used for rejections
    // caused by exhausting some resource (use RESOURCE_EXHAUSTED
    // instead for those errors).  PERMISSION_DENIED must not be
    // used if the caller can not be identified (use UNAUTHENTICATED
    // instead for those errors).
    PERMISSION_DENIED = 7,

    // The request does not have valid authentication credentials for the
    // operation.
    UNAUTHENTICATED = 16,

    // Some resource has been exhausted, perhaps a per-user quota, or
    // perhaps the entire file system is out of space.
    RESOURCE_EXHAUSTED = 8,

    // Operation was rejected because the system is not in a state
    // required for the operation's execution.  For example, directory
    // to be deleted may be non-empty, an rmdir operation is applied to
    // a non-directory, etc.
    //
    // A litmus test that may help a service implementor in deciding
    // between FAILED_PRECONDITION, ABORTED, and UNAVAILABLE:
    //  (a) Use UNAVAILABLE if the client can retry just the failing call.
    //  (b) Use ABORTED if the client should retry at a higher-level
    //      (e.g., restarting a read-modify-write sequence).
    //  (c) Use FAILED_PRECONDITION if the client should not retry until
    //      the system state has been explicitly fixed.  E.g., if an "rmdir"
    //      fails because the directory is non-empty, FAILED_PRECONDITION
    //      should be returned since the client should not retry unless
    //      they have first fixed up the directory by deleting files from it.
    //  (d) Use FAILED_PRECONDITION if the client performs conditional
    //      REST Get/Update/Delete on a resource and the resource on the
    //      server does not match the condition. E.g., conflicting
    //      read-modify-write on the same resource.
    FAILED_PRECONDITION = 9,

    // The operation was aborted, typically due to a concurrency issue
    // like sequencer check failures, transaction aborts, etc.
    //
    // See litmus test above for deciding between FAILED_PRECONDITION,
    // ABORTED, and UNAVAILABLE.
    ABORTED = 10,

    // Operation tried to iterate past the valid input range.  E.g., seeking or
    // reading past end of file.
    //
    // Unlike INVALID_ARGUMENT, this error indicates a problem that may
    // be fixed if the system state changes. For example, a 32-bit file
    // system will generate INVALID_ARGUMENT if asked to read at an
    // offset that is not in the range [0,2^32-1], but it will generate
    // OUT_OF_RANGE if asked to read from an offset past the current
    // file size.
    //
    // There is a fair bit of overlap between FAILED_PRECONDITION and
    // OUT_OF_RANGE.  We recommend using OUT_OF_RANGE (the more specific
    // error) when it applies so that callers who are iterating through
    // a space can easily look for an OUT_OF_RANGE error to detect when
    // they are done.
    OUT_OF_RANGE = 11,

    // Operation is not implemented or not supported/enabled in this service.
    UNIMPLEMENTED = 12,

    // Internal errors.  Means some invariants expected by underlying
    // system has been broken.  If you see one of these errors,
    // something is very broken.
    INTERNAL = 13,

    // The service is currently unavailable.  This is a most likely a
    // transient condition and may be corrected by retrying with
    // a backoff.
    //
    // See litmus test above for deciding between FAILED_PRECONDITION,
    // ABORTED, and UNAVAILABLE.
    UNAVAILABLE = 14,

    // Unrecoverable data loss or corruption.
    DATA_LOSS = 15,

    // An extra enum entry to prevent people from writing code that
    // fails to compile when a new code is added.
    //
    // Nobody should ever reference this enumeration entry. In particular,
    // if you write C++ code that switches on this enumeration, add a default:
    // case instead of a case that mentions this enumeration entry.
    //
    // Nobody should rely on the value (currently 20) listed here.  It
    // may change in the future.
    DO_NOT_USE_RESERVED_FOR_FUTURE_EXPANSION_USE_DEFAULT_IN_SWITCH_INSTEAD_ = 20,
  };
}

class Status {
  public:
    /// Create a success status.
    Status() : state_(nullptr) {}
    ~Status() { delete state_; }

    /// \brief Create a status with the specified error code and msg as a
    /// human-readable string containing more detailed information.
  Status(error::Code code, std::string msg);

    /// Copy the specified status.
    Status(const Status& s);
    void operator=(const Status& s);

    static Status OK() { return Status(); }

    /// Returns true iff the status indicates success.
    bool ok() const { return (state_ == NULL); }

    error::Code code() const {
      return ok() ? error::OK : state_->code;
    }

  const std::string& error_message() const {
      return ok() ? empty_string() : state_->msg;
    }

    bool operator==(const Status& x) const;
    bool operator!=(const Status& x) const;

    /// \brief If `ok()`, stores `new_status` into `*this`.  If `!ok()`,
    /// preserves the current status, but may augment with additional
    /// information about `new_status`.
    ///
    /// Convenient way of keeping track of the first error encountered.
    /// Instead of:
    ///   `if (overall_status.ok()) overall_status = new_status`
    /// Use:
    ///   `overall_status.Update(new_status);`
    void Update(const Status& new_status);

    /// \brief Return a string representation of this status suitable for
    /// printing. Returns the string `"OK"` for success.
  std::string ToString() const;

    // Ignores any errors. This method does nothing except potentially suppress
    // complaints from any tools that are checking that errors are not dropped on
    // the floor.
    void IgnoreError() const;

  private:
  static const std::string& empty_string();
    struct State {
      error::Code code;
      std::string msg;
    };
    // OK status has a `NULL` state_.  Otherwise, `state_` points to
    // a `State` structure containing the error code and message(s)
    State* state_;

    void SlowCopyFrom(const State* src);
  };

inline Status::Status(const Status& s)
  : state_((s.state_ == NULL) ? NULL : new State(*s.state_)) {}

  inline void Status::operator=(const Status& s) {
    // The following condition catches both aliasing (when this == &s),
    // and the common case where both s and *this are ok.
    if (state_ != s.state_) {
      SlowCopyFrom(s.state_);
    }
  }

  inline bool Status::operator==(const Status& x) const {
    return (this->state_ == x.state_) || (ToString() == x.ToString());
  }

  inline bool Status::operator!=(const Status& x) const { return !(*this == x); }

  /// @ingroup core
  std::ostream& operator<<(std::ostream& os, const Status& x);

extern std::string* TfCheckOpHelperOutOfLine(
                                                      const ::Status& v, const char* msg);
inline std::string* TfCheckOpHelper(::Status v,
                                             const char* msg) {
    if (v.ok()) return nullptr;
    return TfCheckOpHelperOutOfLine(v, msg);
  }
#define TF_CHECK_OK(val)                                             \
while (::string* _result = TfCheckOpHelper(val, #val)) \
LOG(FATAL) << *(_result)

namespace errors {

  typedef ::error::Code Code;

  // For propagating errors when calling a function.
#define TF_RETURN_IF_ERROR(expr)                         \
do {                                                   \
const ::Status _status = (expr);         \
if (!_status.ok()) return _status; \
} while (0)

  // Convenience functions for generating and using error status.
  // Example usage:
  //   status.Update(errors::InvalidArgument("The ", foo, " isn't right."));
  //   if (errors::IsInvalidArgument(status)) { ... }
  //   switch (status.code()) { case error::INVALID_ARGUMENT: ... }


template< typename ... Args >
std::string stringer(Args const& ... args ) {
    std::ostringstream stream;
    using List= int[];
    (void)List{0, ( (void)(stream << args), 0 ) ... };
    return stream.str();
  }

#define DECLARE_ERROR(FUNC, CONST)                                       \
  template <typename... Args> \
  ::Status FUNC(Args... args) {                              \
return Status(::error::CONST,              \
stringer(args...)); \
}                   \
inline bool Is##FUNC(const ::Status& status) {             \
return status.code() == ::error::CONST;                  \
}

  DECLARE_ERROR(Cancelled, CANCELLED)
  DECLARE_ERROR(InvalidArgument, INVALID_ARGUMENT)
  DECLARE_ERROR(NotFound, NOT_FOUND)
  DECLARE_ERROR(AlreadyExists, ALREADY_EXISTS)
  DECLARE_ERROR(ResourceExhausted, RESOURCE_EXHAUSTED)
  DECLARE_ERROR(Unavailable, UNAVAILABLE)
  DECLARE_ERROR(FailedPrecondition, FAILED_PRECONDITION)
  DECLARE_ERROR(OutOfRange, OUT_OF_RANGE)
  DECLARE_ERROR(Unimplemented, UNIMPLEMENTED)
  DECLARE_ERROR(Internal, INTERNAL)
  DECLARE_ERROR(Aborted, ABORTED)
  DECLARE_ERROR(DeadlineExceeded, DEADLINE_EXCEEDED)
  DECLARE_ERROR(DataLoss, DATA_LOSS)
  DECLARE_ERROR(Unknown, UNKNOWN)
  DECLARE_ERROR(PermissionDenied, PERMISSION_DENIED)
  DECLARE_ERROR(Unauthenticated, UNAUTHENTICATED)

#undef DECLARE_ERROR

  // The CanonicalCode() for non-errors.
  using ::error::OK;

}  // namespace errors

#endif  // STATUS_H_
